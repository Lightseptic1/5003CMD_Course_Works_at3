#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <openssl/des.h>

using namespace std;

class MorrisThompson16 {
private:
    static string toHex(const unsigned char* data, int length) {
        stringstream ss;
        for (int i = 0; i < length; i++) {
            ss << uppercase << hex << setw(2) << setfill('0') << (int)data[i];
        }
        return ss.str();
    }

    static unsigned short randomSalt() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<unsigned short> dist(0, 0xFFFF);
        return dist(gen);
    }

    static void makeDESKey(const string& password, DES_cblock& keyBlock) {
        unsigned char temp[8] = {0};

        for (size_t i = 0; i < 8 && i < password.size(); i++) {
            unsigned char c = static_cast<unsigned char>(password[i]);

            // Keep only low 7 bits, then shift left for DES parity bit
            unsigned char candidate = (c & 0x7F) << 1;

            // Set odd parity
            int ones = 0;
            for (int b = 0; b < 8; b++) {
                if ((candidate >> b) & 1) {
                    ones++;
                }
            }
            if (ones % 2 == 0) {
                candidate |= 0x01;
            }

            temp[i] = candidate;
        }

        for (int i = 0; i < 8; i++) {
            keyBlock[i] = temp[i];
        }
    }

public:
    static string encryptPassword(const string& password, unsigned short salt) {
        DES_cblock keyBlock;
        DES_key_schedule schedule;

        makeDESKey(password, keyBlock);
        DES_set_odd_parity(&keyBlock);
        DES_set_key_checked(&keyBlock, &schedule);

        DES_cblock block;

        // Repeat 16-bit salt four times to create 8-byte block
        unsigned char high = static_cast<unsigned char>((salt >> 8) & 0xFF);
        unsigned char low  = static_cast<unsigned char>(salt & 0xFF);

        for (int i = 0; i < 8; i += 2) {
            block[i] = high;
            block[i + 1] = low;
        }

        // Encrypt 25 times
        for (int i = 0; i < 25; i++) {
            DES_ecb_encrypt(&block, &block, &schedule, DES_ENCRYPT);
        }

        stringstream result;
        result << uppercase << hex << setw(4) << setfill('0') << salt;
        result << "$" << toHex(reinterpret_cast<unsigned char*>(block), 8);

        return result.str();
    }

    static string encryptPassword(const string& password) {
        return encryptPassword(password, randomSalt());
    }

    static bool checkPassword(const string& password, const string& storedHash) {
        size_t pos = storedHash.find('$');
        if (pos == string::npos) {
            return false;
        }

        string saltHex = storedHash.substr(0, pos);

        unsigned short salt;
        stringstream ss;
        ss << hex << saltHex;
        ss >> salt;

        string recomputed = encryptPassword(password, salt);
        return recomputed == storedHash;
    }
};

int main() {
    vector<string> passwords = {
        "alpha123",
        "delta789",
        "unixpass",
        "comp5003",
        "network1",
        "oslab2026",
        "kernel42",
        "secureme",
        "student7",
        "thompson"
    };

    cout << "Generated list of 10 encrypted passwords:\n\n";
    vector<string> encryptedList;

    for (const string& pw : passwords) {
        string enc = MorrisThompson16::encryptPassword(pw);
        encryptedList.push_back(enc);
        cout << left << setw(12) << pw << " -> " << enc << "\n";
    }

    cout << "\nPassword verification test:\n";
    string testPassword;
    string storedHash;

    cout << "Enter password to test: ";
    cin >> testPassword;

    cout << "Enter stored hash: ";
    cin >> storedHash;

    if (MorrisThompson16::checkPassword(testPassword, storedHash)) {
        cout << "Password is valid.\n";
    } else {
        cout << "Password is NOT valid.\n";
    }

    return 0;
}