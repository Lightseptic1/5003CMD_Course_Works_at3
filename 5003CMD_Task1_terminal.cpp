#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#include <cerrno>

using namespace std;

extern char **environ;

struct Cmd { // Single parsed command line
    vector<string> args;
    string inFile;
    string outFile;
    bool append = false;
    bool background = false;
};

string getCwd() {
    char* p = getcwd(nullptr, 0);
    if (!p) return "";
    string s(p);
    free(p);
    return s;
}

vector<string> splitWS(const string& line) {
    istringstream iss(line);
    vector<string> t;
    string s;
    while (iss >> s) t.push_back(s);
    return t;
}

bool parseLine(const string& line, Cmd& c) {
    c = Cmd{};
    vector<string> t = splitWS(line);
    if (t.empty()) return false;

    if (t.back() == "&") {
        c.background = true;
        t.pop_back();
    }

    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == "<") {
            if (i + 1 >= t.size()) {
                std::cerr << "Error: missing input file after <\n";
                return false;
            }
            c.inFile = t[++i];
        } else if (t[i] == ">" || t[i] == ">>") {
            if (i + 1 >= t.size()) {
                std::cerr << "Error: missing output file after > or >>\n";
                return false;
            }
            c.append = (t[i] == ">>");
            c.outFile = t[++i];
        } else {
            c.args.push_back(t[i]);
        }
    }

    return !c.args.empty();
}

enum Builtin {
cd,
dir,
environ_1,
set,
echo,
help,
pause_1,
quit,
none,
};

Builtin identify(const string& s) {
    if (s == "cd") return cd;
    if (s == "dir") return dir;
    if (s == "environ") return environ_1;
    if (s == "set") return set;
    if (s == "echo") return echo;
    if (s == "help") return help;
    if (s == "pause") return pause_1;
    if (s == "quit") return quit;
    return none;
}

int openOutFile(const string& path, bool append) {
    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    return open(path.c_str(), flags, 0644);
}

bool redirectStdoutForBuiltin(const Cmd& c, int& savedStdout) {
    savedStdout = -1;
    if (c.outFile.empty()) return true;

    int fd = openOutFile(c.outFile, c.append);
    if (fd < 0) {
        std::cerr << "Error: cannot open " << c.outFile << ": " << strerror(errno) << "\n";
        return false;
    }

    savedStdout = dup(STDOUT_FILENO);
    if (savedStdout < 0) {
        std::cerr << "Error: dup failed: " << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        std::cerr << "Error: dup2 failed: " << strerror(errno) << "\n";
        close(fd);
        close(savedStdout);
        savedStdout = -1;
        return false;
    }

    close(fd);
    return true;
}

void restoreStdout(int savedStdout) {
    if (savedStdout >= 0) {
        cout.flush();
        dup2(savedStdout, STDOUT_FILENO);
        close(savedStdout);
    }
}

void builtin_dir(const string& dir) {
    DIR* d = opendir(dir.c_str());
    if (!d) {
        std::cerr << "dir error: " << strerror(errno) << "\n";
        return;
    }

    dirent* e;
    while ((e = readdir(d)) != nullptr) {
        cout << e->d_name << "\n";
    }

    closedir(d);
}

void builtin_environ() {
    for (char** e = environ; *e; e++) {
        cout << *e << "\n";
    }
}

void builtin_echo(const vector<string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        if (i > 1) cout << " ";
        cout << args[i];
    }
    cout << "\n";
}

string buildManualText() {
    ostringstream out;
    out << "cd [DIR]       - Change the current directory to DIR (or print it if DIR is not given)\n";
    out << "dir [DIR]      - List the contents of directory DIR (or current directory if DIR is not given)\n";
    out << "environ        - Print all environment variables\n";
    out << "set VAR VALUE  - Set environment variable VAR to VALUE\n";
    out << "echo [COMMENT] - Print ARGS to standard output\n";
    out << "help           - Show this help message\n";
    out << "pause          - Wait for the user to press Enter\n";
    out << "quit           - Quit the shell\n";
    return out.str();
}

void builtin_help_more() {
    int pfd[2];
    if (pipe(pfd) < 0) {
        std::cerr << "help error: pipe failed: " << strerror(errno) << "\n";
        return;
    }

    pid_t pid = fork();
    switch (pid) {
        case -1: {
            std::cerr << "help error: fork failed: " << strerror(errno) << "\n";
            close(pfd[0]);
            close(pfd[1]);
            return;
        }

        case 0: {
            if (dup2(pfd[0], STDIN_FILENO) < 0) {
                std::cerr << "help error: dup2 failed: " << strerror(errno) << "\n";
                _exit(1);
            }
            close(pfd[0]);
            close(pfd[1]);

            char* const argvMore[] = { (char*)"more", nullptr };
            execvp(argvMore[0], argvMore);

            std::cerr << "help error: execvp failed: " << strerror(errno) << "\n";
            _exit(1);
        }

        default: {
            close(pfd[0]);

            std::string manual = buildManualText();
            ssize_t wrote = write(pfd[1], manual.c_str(), manual.size());
            (void)wrote;

            close(pfd[1]);
            waitpid(pid, nullptr, 0);
            return;
        }
    }
}
   

void execExternal(const Cmd& c) {
    pid_t pid = fork();

    switch (pid) {
        case -1: {
            std::cerr << "fork failed: " << strerror(errno) << "\n";
            return;
        }

        case 0: {
            if (!c.inFile.empty()) {
                int fd = open(c.inFile.c_str(), O_RDONLY);
                if (fd < 0) {
                    std::cerr << "Cannot open " << c.inFile << ": " << strerror(errno) << "\n";
                    _exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (!c.outFile.empty()) {
                int fd = openOutFile(c.outFile, c.append);
                if (fd < 0) {
                    std::cerr << "Cannot open " << c.outFile << ": " << strerror(errno) << "\n";
                    _exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            vector<char*> argv;
            argv.reserve(c.args.size() + 1);
            for (const string& s : c.args) argv.push_back(const_cast<char*>(s.c_str()));
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            std::cerr << "Command not found: " << c.args[0] << "\n";
            _exit(1);
        }

        default: {
            if (!c.background) {
                waitpid(pid, nullptr, 0);
            } else {
                cout << "[bg pid " << pid << "]\n";
            }
            return;
        }
    }
}

bool runCommand(const Cmd& c) {
    Builtin b = identify(c.args[0]);

    bool needsBuiltinStdoutRedirect =
        (b == dir || b == environ_1 || b == echo || b == help);

    int savedStdout = -1;
    if (needsBuiltinStdoutRedirect) {
        if (!redirectStdoutForBuiltin(c, savedStdout)) return true;
    }

    switch (b) {
        case cd: {
            if (c.args.size() == 1) {
                cout << getCwd() << "\n";
            } else {
                if (chdir(c.args[1].c_str()) != 0) {
                    std::cerr << "cd error: " << strerror(errno) << "\n";
                } else {
                    string now = getCwd();
                    setenv("PWD", now.c_str(), 1);
                }
            }
        } break;

        case dir: {
            string d = (c.args.size() >= 2) ? c.args[1] : ".";
            builtin_dir(d);
        } break;

        case environ_1:
            builtin_environ();
            break;

        case set:
            if (c.args.size() >= 3) setenv(c.args[1].c_str(), c.args[2].c_str(), 1);
            else std::cerr << "set usage: set VARIABLE VALUE\n";
            break;

        case echo:
            builtin_echo(c.args);
            break;

        case help:
            builtin_help_more();
            break;

        case pause_1: {
            cout << "Press Enter to continue...";
            cout.flush();
            string dummy;
            getline(cin, dummy);
        } break;

        case quit:
            if (needsBuiltinStdoutRedirect) restoreStdout(savedStdout);
            return false;

        case none:
            if (needsBuiltinStdoutRedirect) restoreStdout(savedStdout);
            execExternal(c);
            return true;
    }

    if (needsBuiltinStdoutRedirect) restoreStdout(savedStdout);
    return true;
}

int main(int argc, char** argv) {
    setenv("PWD", getCwd().c_str(), 1);

    istream* input = &cin;
    ifstream batch;
    bool interactive = true;

    if (argc >= 2) {
        batch.open(argv[1]);
        if (!batch.is_open()) {
            std::cerr << "Cannot open batch file: " << argv[1] << "\n";
            return 1;
        }
        input = &batch;
        interactive = false;
    }

    string line;
    while (true) {
        if (interactive) {
            cout << getCwd() << " $ ";
            cout.flush();
        }

        if (!getline(*input, line)) break;

        Cmd c;
        if (!parseLine(line, c)) continue;

        if (!runCommand(c)) break;
    }

    return 0;
}
