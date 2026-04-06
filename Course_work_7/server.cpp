#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <sstream>
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

struct ClientInfo {
    int socket;
    std::string nickname;
};

std::vector<ClientInfo> clients;
std::mutex clientsMutex;

std::string getCurrentTime() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%I:%M %p", &localTime);

    std::string timeStr(buffer);
    if (!timeStr.empty() && timeStr[0] == '0') {
        timeStr.erase(0, 1);
    }

    return timeStr;
}

void sendToClient(int clientSocket, const std::string& message) {
    send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
}

void broadcastMessage(const std::string& message, int senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);

    for (const auto& client : clients) {
        if (client.socket != senderSocket) {
            sendToClient(client.socket, message);
        }
    }
}
void removeClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);

    clients.erase(
        std::remove_if(
            clients.begin(),
            clients.end(),
            [clientSocket](const ClientInfo& client) {
                return client.socket == clientSocket;
            }
        ),
        clients.end()
    );
}

bool extractLine(std::string& accumulatedData, std::string& line) {
    size_t newlinePos = accumulatedData.find('\n');
    if (newlinePos == std::string::npos) {
        return false;
    }

    line = accumulatedData.substr(0, newlinePos);
    accumulatedData.erase(0, newlinePos + 1);
    return true;
}

void handleClient(int clientSocket) {
    char buffer[1024];
    std::string accumulatedData;
    std::string nickname;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived <= 0) {
            break;
        }

        buffer[bytesReceived] = '\0';
        accumulatedData += buffer;

        std::string line;
        while (extractLine(accumulatedData, line)) {
            if (nickname.empty()) {
                nickname = line;

                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.push_back({clientSocket, nickname});
                }

                std::string joinMessage = "[SERVER] " + nickname + " joined the chat.\n";
                std::cout << joinMessage;
                broadcastMessage(joinMessage, clientSocket);
            } else {
                std::string timestamp = getCurrentTime();
                std::string formattedMessage = nickname + " (" + timestamp + "): " + line + "\n";

                std::cout << formattedMessage;
                broadcastMessage(formattedMessage, clientSocket);
            }
        }
    }

    if (!nickname.empty()) {
        std::string leaveMessage = "[SERVER] " + nickname + " left the chat.\n";
        std::cout << leaveMessage;
        broadcastMessage(leaveMessage, clientSocket);
    }

    removeClient(clientSocket);

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8086);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed.\n";
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Listen failed.\n";
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return 1;
    }

    std::cout << "Server listening on 127.0.0.1:8086...\n";

    while (true) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientSize = sizeof(clientAddr);
#else
        socklen_t clientSize = sizeof(clientAddr);
#endif

        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientSize);

        if (clientSocket < 0) {
            std::cerr << "Accept failed.\n";
            continue;
        }

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif

    return 0;
}
//g++ server.cpp -o server -lws2_32
//g++ client.cpp -o client -lws2_32