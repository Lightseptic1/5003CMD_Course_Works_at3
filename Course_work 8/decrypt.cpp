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
    // Traditional DES-crypt alphabet
    static constexpr const char* SALT_ALPHABET =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    static unsigned short randomSalt16() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<unsigned short> dist(0, 0xFFFF);
        return dist(gen);
    }

    // Convert full 16-bit salt into a 2-char DES-crypt salt.
    // This preserves the assignment's 16-bit salt storage while producing
    // the 2-character salt required by DES_fcrypt().
    static string salt16ToDesSalt(unsigned short salt16) {
        char saltChars[3];
        saltChars[0] = SALT_ALPHABET[salt16 & 0x3F];          // lower 6 bits
        saltChars[1] = SALT_ALPHABET[(salt16 >> 6) & 0x3F];   // next 6 bits
        saltChars[2] = '\0';
        return string(saltChars);
    }

    static string salt16ToHex(unsigned short salt16) {
        stringstream ss;
        ss << uppercase << hex << setw(4) << setfill('0') << salt16;
        return ss.str();
    }

    static bool parseSaltHex(const string& saltHex, unsigned short& saltOut) {
        if (saltHex.size() != 4) {
            return false;
        }

        stringstream ss(saltHex);
        ss >> hex >> saltOut;
        return !ss.fail();
    }

public:
    static string encryptPassword(const string& password, unsigned short salt16) {
        string desSalt = salt16ToDesSalt(salt16);

        char output[32] = {0};

        // DES_fcrypt implements the traditional DES-based UNIX password hash
        // behavior rather than raw ECB encryption.
        if (DES_fcrypt(password.c_str(), desSalt.c_str(), output) == nullptr) {
            throw runtime_error("DES_fcrypt failed.");
        }

        // Store full 16-bit salt for the coursework requirement,
        // followed by the DES-crypt hash output.
        return salt16ToHex(salt16) + "$" + string(output);
    }

    static string encryptPassword(const string& password) {
        return encryptPassword(password, randomSalt16());
    }

    static bool checkPassword(const string& password, const string& storedHash) {
        size_t pos = storedHash.find('$');
        if (pos == string::npos) {
            return false;
        }

        string saltHex = storedHash.substr(0, pos);
        string storedCrypt = storedHash.substr(pos + 1);

        unsigned short salt16;
        if (!parseSaltHex(saltHex, salt16)) {
            return false;
        }

        string recomputed = encryptPassword(password, salt16);
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