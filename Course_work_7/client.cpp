#include <iostream>
#include <string>
#include <thread>
#include <ctime>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

bool running = true;

bool extractLine(std::string& accumulatedData, std::string& line) {
    size_t newlinePos = accumulatedData.find('\n');
    if (newlinePos == std::string::npos) {
        return false;
    }

    line = accumulatedData.substr(0, newlinePos);
    accumulatedData.erase(0, newlinePos + 1);
    return true;
}

void receiveMessages(int clientSocket) {
    char buffer[1024];
    std::string accumulatedData;

    while (running) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived <= 0) {
            std::cout << "\nDisconnected from server.\n";
            running = false;
            break;
        }

        buffer[bytesReceived] = '\0';
        accumulatedData += buffer;

        std::string line;
        while (extractLine(accumulatedData, line)) {
            std::cout << line << std::endl;
        }
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8086);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed.\n";
#ifdef _WIN32
        closesocket(clientSocket);
        WSACleanup();
#else
        close(clientSocket);
#endif
        return 1;
    }

    std::cout << "Connected to server.\n";

    std::string nickname;
    std::cout << "Enter your nickname: ";
    std::getline(std::cin, nickname);

    if (nickname.empty()) {
        nickname = "Unknown";
    }

    std::string nicknamePacket = nickname + "\n";
    send(clientSocket, nicknamePacket.c_str(), static_cast<int>(nicknamePacket.length()), 0);

    std::thread receiverThread(receiveMessages, clientSocket, nickname);

    std::cout << "Type messages. Type /exit to quit.\n";

while (running) {
    std::string message;
    std::getline(std::cin, message);
    if (message.empty()) {
        continue;
}
    if (!running) {
        break;
    }

    if (message == "/exit") {
        running = false;
        break;
    }

    std::string toSend = message + "\n";
    int bytesSent = send(clientSocket, toSend.c_str(), static_cast<int>(toSend.length()), 0);

    if (bytesSent < 0) {
        std::cerr << "Send failed." << std::endl;
        running = false;
        break;
    }
}
#ifdef _WIN32
    closesocket(clientSocket);
    WSACleanup();
#else
    close(clientSocket);
#endif

    if (receiverThread.joinable()) {
        receiverThread.join();
    }

    return 0;
}