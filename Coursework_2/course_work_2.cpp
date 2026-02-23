#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <cctype>
#include <functional>
#include <mutex>
using namespace std;
mutex printMutex;
using words_map = unordered_map<string, long long>;
static void add_word(words_map& m, string& w) {
    if (!w.empty()) {
        m[w]++;
        w.clear();
    }
}
static void countline(const string& line, words_map& m) {
    string w;
    w.reserve(32);
    for (unsigned char c : line) {
        if (isalnum(c)){
            w.push_back((char)tolower(c));
        } 
        else{ 
            add_word(m, w); 
        }
    }
    add_word(m, w);
}
static void reader(const vector<string>& lines, int s_line, int e_line, int tid, words_map& out) {
    words_map local;
    local.reserve(4096);

    for (int i = s_line; i < e_line; i++) {
        countline(lines[i], local);
    }

    out = std::move(local);

    {
        lock_guard<mutex> lock(printMutex);
        cout << "\nIntermediate segment result for thread " << tid
             << " lines [" << s_line << ", " << e_line << ")\n";
        for (const auto& kv : out) {
            cout << kv.first << ": " << kv.second << "\n";
        }
        cout << flush;
    }
}
static void merge_into(words_map& dst, const words_map& src) {
    dst.reserve(dst.size() + src.size());
    for (const auto& [word, cnt] : src) {
        dst[word] += cnt;
    }
}
static words_map parallel_merge(vector<words_map>& partial) {
    int active = (int)partial.size();
    if (active == 0) return {};
    if (active == 1) return std::move(partial[0]);

    while (active > 1) {
        int pairs = active / 2;
        int leftover = active % 2;

        vector<thread> mergeThreads;
        mergeThreads.reserve(pairs);

        for (int i = 0; i < pairs; i++) {
            int dstIdx = 2 * i;
            int srcIdx = 2 * i + 1;
            mergeThreads.emplace_back([&partial, dstIdx, srcIdx]() {
                merge_into(partial[dstIdx], partial[srcIdx]);
            });
        }
        for (auto& t : mergeThreads) t.join();

        vector<words_map> next;
        next.reserve(pairs + leftover);

        for (int i = 0; i < pairs; i++) {
            next.push_back(std::move(partial[2 * i])); 
        }
        if (leftover) {
            next.push_back(std::move(partial[active - 1]));
        }

        partial = std::move(next);
        active = (int)partial.size();
    }

    return std::move(partial[0]);
}
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        cerr << "Usage: " << argv[0] << " <file.txt> <N>\n";
        return 1;
    }
string filename = argv[1];

int cores = (int)thread::hardware_concurrency();
if (cores <= 0) cores = 1;

int N;

if (argc == 3) {
    try {
        N = stoi(argv[2]);
        if (N <= 0) throw std::invalid_argument("non-positive");
    } catch (...) {
        cerr << "N must be a positive integer\n";
        return 1;
    }
} else {
    N = cores;  
}
    filename = argv[1];
    ifstream in(filename);
    if (!in) {
        cerr << "Failed to open file\n";
        return 1;
    }
    vector<string> lines;
    string line;
    while (getline(in, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) {
        cerr << "Empty file\n";
        return 1;
    }
    
    if (N > (int)lines.size()) N = (int)lines.size();
    vector<words_map> partial(N);
    vector<int> segStart(N), segEnd(N);
    vector<thread> threads;
    threads.reserve(N);
    int base = (int)lines.size() / N;
    int extra = (int)lines.size() % N;
    int start = 0;
    for (int i = 0; i < N; i++) {
        int len = base + (i < extra ? 1 : 0);
        int end = start + len;
        segStart[i] = start;
        segEnd[i] = end;
        threads.emplace_back(reader, std::cref(lines), start, end, i, std::ref(partial[i]));
        start = end;
    }
    for (auto& t : threads) {
        t.join();
    }

    words_map global = parallel_merge(partial);
    cout << "\n===== Merged =====\n";
    for (const auto& [word, cnt] : global) {
        cout << word << ": " << cnt << "\n";
    }
    return 0;
}
//g++ -std=c++17 -O3 -pthread course_work_2.cpp -o course_work_2