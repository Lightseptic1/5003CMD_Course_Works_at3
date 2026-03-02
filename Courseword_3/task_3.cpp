#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

struct Frame {
    long long page = -1;
    uint32_t age = 0;
    bool referenced = false;
    bool loaded = false;
    uint64_t birth = 0; // tie breaker, older birth evicts first if ages equal
};

static void die(const std::string& msg) {
    std::cerr << msg << "\n";
    std::exit(1);
}

static std::vector<long long> read_trace(const std::string& path) {
    std::ifstream in(path);
    if (!in) die("Failed to open input file: " + path);

    std::vector<long long> trace;
    trace.reserve(1 << 20);

    long long p;
    while (in >> p) trace.push_back(p);

    if (trace.empty()) die("Input file contained no page references.");
    return trace;
}

static inline void aging_tick(std::vector<Frame>& frames, uint32_t top_bit_mask) {
    for (auto& f : frames) {
        if (!f.loaded) continue;
        f.age >>= 1;
        if (f.referenced) f.age |= top_bit_mask;
        f.referenced = false;
    }
}

static double simulate_aging_faults_per_1000(const std::vector<long long>& trace, int num_frames, int counter_bits = 8, int tick_every = 1) {
    if (num_frames <= 0) die("num_frames must be positive.");
    if (counter_bits <= 0 || counter_bits > 32) die("counter_bits must be between 1 and 32.");
    if (tick_every <= 0) die("tick_every must be positive.");

    uint32_t top_bit_mask = 1u << (counter_bits - 1);

    std::vector<Frame> frames(static_cast<size_t>(num_frames));
    std::unordered_map<long long, int> where;
    where.reserve(static_cast<size_t>(num_frames) * 2);

    uint64_t faults = 0;
    uint64_t t = 0;

    for (size_t i = 0; i < trace.size(); ++i) {
        long long page = trace[i];

        auto it = where.find(page);
        if (it != where.end()) {
            frames[it->second].referenced = true;
        } else {
            faults++;

            int free_idx = -1;
            for (int fi = 0; fi < num_frames; ++fi) {
                if (!frames[fi].loaded) { free_idx = fi; break; }
            }

            int use_idx = free_idx;

            if (use_idx == -1) {
                uint32_t best_age = std::numeric_limits<uint32_t>::max();
                uint64_t best_birth = std::numeric_limits<uint64_t>::max();
                int victim = 0;

                for (int fi = 0; fi < num_frames; ++fi) {
                    const auto& f = frames[fi];
                    if (f.age < best_age || (f.age == best_age && f.birth < best_birth)) {
                        best_age = f.age;
                        best_birth = f.birth;
                        victim = fi;
                    }
                }

                where.erase(frames[victim].page);
                use_idx = victim;
            }

            Frame& f = frames[use_idx];
            f.page = page;
            f.loaded = true;
            f.referenced = true;
            f.age = 0;
            f.birth = t++;

            where[page] = use_idx;
        }

        if (((static_cast<int>(i) + 1) % tick_every) == 0) {
            aging_tick(frames, top_bit_mask);
        }
    }

    double refs = static_cast<double>(trace.size());
    return (static_cast<double>(faults) / refs) * 1000.0;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr
            << "Usage:\n"
            << "  " << argv[0] << " <trace_file> <min_frames> <max_frames> [step] [bits] [tick_every]\n\n"
            << "Example:\n"
            << "  " << argv[0] << " trace.txt 2 128 2 8 1\n";
        return 1;
    }

    std::string trace_path = argv[1];
    int min_frames = std::stoi(argv[2]);
    int max_frames = std::stoi(argv[3]);
    int step = (argc >= 5) ? std::stoi(argv[4]) : 1;
    int bits = (argc >= 6) ? std::stoi(argv[5]) : 8;
    int tick_every = (argc >= 7) ? std::stoi(argv[6]) : 1;

    if (min_frames <= 0 || max_frames <= 0 || step <= 0) die("Frame counts and step must be positive.");
    if (min_frames > max_frames) die("min_frames must be <= max_frames.");

    auto trace = read_trace(trace_path);

    std::ofstream csv("results.csv");
    if (!csv) die("Failed to create results.csv");
    csv << "frames,faults_per_1000\n";

    std::cout << "Trace references: " << trace.size() << "\n";
    std::cout << "frames,faults_per_1000\n";

    for (int f = min_frames; f <= max_frames; f += step) {
        double v = simulate_aging_faults_per_1000(trace, f, bits, tick_every);
        std::cout << f << "," << v << "\n";
        csv << f << "," << v << "\n";
    }

    std::cout << "Wrote results.csv\n";
    return 0;
}
//g++ -O2 -std=c++17 -Wall -Wextra -pedantic task_3.cpp -o aging_sim
// ex trace numbers 2 16 2 8 1
// 2 64 2 8 1