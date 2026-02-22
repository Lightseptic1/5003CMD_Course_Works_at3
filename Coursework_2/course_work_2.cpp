#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <cctype>
using namespace std;
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
        if (isalnum(c)) w.push_back((char)tolower(c));
        else add_word(m, w);
    }
    add_word(m, w);
}
static void reader(const vector<string>& lines, int s_line, int e_line, words_map& out) {
    words_map local;
    local.reserve(4096);
    for (int i = s_line; i < e_line; i++) {
        countline(lines[i], local);
    }
    out = move(local);
}
static void merge_into(words_map& dst, const words_map& src) {
    dst.reserve(dst.size() + src.size());
    for (const auto& [word, cnt] : src) {
        dst[word] += cnt;
    }
}
static words_map parallel_merge(vector<words_map>& partial) {
    int active = (int)partial.size();
    if (active == 0){
         return {};
    }
    if (active == 1) {
        return move(partial[0]);
    }
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
        for (auto& t : mergeThreads) {
            t.join();
        }
        int write = 0;
        for (int read = 0; read < active; read += 2) {
            if (write != read) partial[write] = move(partial[read]);
            write++;
        }
        if (leftover) {
            int last = active - 1;
            if (write != last) partial[write] = move(partial[last]);
            write++;
        }
        active = write;
        partial.resize(active);
    }
    return move(partial[0]);
}
int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <file.txt> <N>\n";
        return 1;
    }
    string filename = argv[1];
    int N = 0;
    try {
        N = stoi(argv[2]);
    } catch (...) {
        cerr << "N must be an integer\n";
        return 1;
    }
    if (N <= 0) {
        return 1;
    }
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
    int cores = (int)thread::hardware_concurrency();
    if (cores <= 0) cores = 1;
    if (N > cores) N = cores;
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
        threads.emplace_back(reader, cref(lines), start, end, ref(partial[i]));
        start = end;
    }
    for (auto& t : threads) {
        t.join();
    }
    for (int i = 0; i < N; i++) {
        cout << "\nThread " << i << " segment lines [" << segStart[i] << ", " << segEnd[i] << ")\n";
        for (const auto& [word, cnt] : partial[i]) {
            cout << word << ": " << cnt << "\n";
        }
    }
    words_map global = parallel_merge(partial);
    cout << "\n===== Merged =====\n";
    for (const auto& [word, cnt] : global) {
        cout << word << ": " << cnt << "\n";
    }
    return 0;
}