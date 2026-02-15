#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#include <cerrno>

using namespace std;

extern char **environ;

struct Cmd {
    vector<string> args;
    string inFile;
    string outFile;
    bool append = false;
    bool background = false;
};

string getCwd() {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) return "";
    return string(buf);
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
                cerr << "Error: missing input file after <\n";
                return false;
            }
            c.inFile = t[++i];
        } else if (t[i] == ">" || t[i] == ">>") {
            if (i + 1 >= t.size()) {
                cerr << "Error: missing output file after > or >>\n";
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
    BI_CD, BI_DIR, BI_ENVIRON, BI_SET, BI_ECHO, BI_HELP, BI_PAUSE, BI_QUIT, BI_NONE
};

Builtin identify(const string& s) {
    if (s == "cd") return BI_CD;
    if (s == "dir") return BI_DIR;
    if (s == "environ") return BI_ENVIRON;
    if (s == "set") return BI_SET;
    if (s == "echo") return BI_ECHO;
    if (s == "help") return BI_HELP;
    if (s == "pause") return BI_PAUSE;
    if (s == "quit") return BI_QUIT;
    return BI_NONE;
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
        cerr << "Error: cannot open " << c.outFile << ": " << strerror(errno) << "\n";
        return false;
    }

    savedStdout = dup(STDOUT_FILENO);
    if (savedStdout < 0) {
        cerr << "Error: dup failed: " << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        cerr << "Error: dup2 failed: " << strerror(errno) << "\n";
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
        cerr << "dir error: " << strerror(errno) << "\n";
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
    out << "myshell user manual\n\n";
    out << "Built in commands\n";
    out << "  cd [DIRECTORY]\n";
    out << "    Change directory. If DIRECTORY is missing, print current directory.\n";
    out << "  dir DIRECTORY\n";
    out << "    List the contents of DIRECTORY.\n";
    out << "  environ\n";
    out << "    List all environment variables.\n";
    out << "  set VARIABLE VALUE\n";
    out << "    Set VARIABLE to VALUE.\n";
    out << "  echo [COMMENT]\n";
    out << "    Print COMMENT with spaces normalized.\n";
    out << "  help\n";
    out << "    Display this manual using more.\n";
    out << "  pause\n";
    out << "    Wait until Enter is pressed.\n";
    out << "  quit\n";
    out << "    Exit the shell.\n\n";
    out << "Features\n";
    out << "  Input redirection:  program < inputfile\n";
    out << "  Output redirection: program > outputfile or program >> outputfile\n";
    out << "  Background run: add & at end of command line\n";
    return out.str();
}

void builtin_help_more() {
    int pfd[2];
    if (pipe(pfd) < 0) {
        cerr << "help error: pipe failed: " << strerror(errno) << "\n";
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        cerr << "help error: fork failed: " << strerror(errno) << "\n";
        close(pfd[0]);
        close(pfd[1]);
        return;
    }

    if (pid == 0) {
        dup2(pfd[0], STDIN_FILENO);
        close(pfd[0]);
        close(pfd[1]);

        char* const argvMore[] = { (char*)"more", nullptr };
        execvp(argvMore[0], argvMore);
        _exit(1);
    }

    close(pfd[0]);

    string manual = buildManualText();
    write(pfd[1], manual.c_str(), manual.size());
    close(pfd[1]);

    waitpid(pid, nullptr, 0);
}

void execExternal(const Cmd& c) {
    pid_t pid = fork();
    if (pid < 0) {
        cerr << "fork failed: " << strerror(errno) << "\n";
        return;
    }

    if (pid == 0) {
        if (!c.inFile.empty()) {
            int fd = open(c.inFile.c_str(), O_RDONLY);
            if (fd < 0) {
                cerr << "Cannot open " << c.inFile << ": " << strerror(errno) << "\n";
                _exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (!c.outFile.empty()) {
            int fd = openOutFile(c.outFile, c.append);
            if (fd < 0) {
                cerr << "Cannot open " << c.outFile << ": " << strerror(errno) << "\n";
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
        cerr << "Command not found: " << c.args[0] << "\n";
        _exit(1);
    }

    if (!c.background) {
        waitpid(pid, nullptr, 0);
    } else {
        cout << "[bg pid " << pid << "]\n";
    }
}

bool runCommand(const Cmd& c) {
    Builtin b = identify(c.args[0]);

    bool needsBuiltinStdoutRedirect =
        (b == BI_DIR || b == BI_ENVIRON || b == BI_ECHO || b == BI_HELP);

    int savedStdout = -1;
    if (needsBuiltinStdoutRedirect) {
        if (!redirectStdoutForBuiltin(c, savedStdout)) return true;
    }

    switch (b) {
        case BI_CD: {
            if (c.args.size() == 1) {
                cout << getCwd() << "\n";
            } else {
                if (chdir(c.args[1].c_str()) != 0) {
                    cerr << "cd error: " << strerror(errno) << "\n";
                } else {
                    string now = getCwd();
                    setenv("PWD", now.c_str(), 1);
                }
            }
        } break;

        case BI_DIR: {
            string d = (c.args.size() >= 2) ? c.args[1] : ".";
            builtin_dir(d);
        } break;

        case BI_ENVIRON:
            builtin_environ();
            break;

        case BI_SET:
            if (c.args.size() >= 3) setenv(c.args[1].c_str(), c.args[2].c_str(), 1);
            else cerr << "set usage: set VARIABLE VALUE\n";
            break;

        case BI_ECHO:
            builtin_echo(c.args);
            break;

        case BI_HELP:
            builtin_help_more();
            break;

        case BI_PAUSE: {
            cout << "Press Enter to continue...";
            cout.flush();
            string dummy;
            getline(cin, dummy);
        } break;

        case BI_QUIT:
            if (needsBuiltinStdoutRedirect) restoreStdout(savedStdout);
            return false;

        case BI_NONE:
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
            cerr << "Cannot open batch file: " << argv[1] << "\n";
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
