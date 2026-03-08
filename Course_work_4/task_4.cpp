#include <iostream>
#include <vector>
#include <fstream>
#include <string>

using namespace std;

bool isFinishable(const vector<int>& available, const vector<int>& request){
    for (size_t i = 0; i < available.size(); ++i) {
        if (request[i] > available[i]) {
            return false;
        }
    }
    return true;
}
int main(int argc, char* argv[]){
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    ifstream fin(argv[1]);
    if (!fin) {
        cerr << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }

    int n, m;
    fin >> n >> m;

    if (!fin || n <= 0 || m <= 0) {
        cerr << "Error: Invalid number of processes or resource types.\n";
        return 1;
    }

    vector<int> E(m);
    for (int j = 0; j < m; j++) {
        fin >> E[j];
        if (!fin || E[j] < 0) {
            cerr << "Error: Invalid value in existing resources vector E.\n";
            return 1;
        }
    }

    vector<vector<int>> C(n, vector<int>(m));
    vector<vector<int>> R(n, vector<int>(m));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            fin >> C[i][j];
            if (!fin || C[i][j] < 0) {
                cerr << "Error: Invalid value in allocation matrix C.\n";
                return 1;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            fin >> R[i][j];
            if (!fin || R[i][j] < 0) {
                cerr << "Error: Invalid value in request matrix R.\n";
                return 1;
            }
        }
    }

    vector<int> available(m, 0);

    for (int j = 0; j < m; j++) {
        int allocatedSum = 0;
        for (int i = 0; i < n; i++) {
            allocatedSum += C[i][j];
        }

        available[j] = E[j] - allocatedSum;

        if (available[j] < 0) {
            cerr << "Error: Allocation exceeds total existing resources for resource type " << j << ".\n";
            return 1;
        }
    }

    vector<bool> finished(n, false);

    bool progressMade = true;
    while (progressMade) {
        progressMade = false;

        for (int i = 0; i < n; i++) {
            if (!finished[i] && isFinishable(available, R[i])) {
                for (int j = 0; j < m; j++) {
                    available[j] += C[i][j];
                }
                finished[i] = true;
                progressMade = true;
            }
        }
    }

    vector<int> deadlockedes;
    for (int i = 0; i < n; i++) {
        if (!finished[i]) {
            deadlockedes.push_back(i);
        }
    }

    if (deadlockedes.empty()) {
        cout << "No deadlock.\n";
    } else {
        cout << "Deadlock!\n";
        cout << "Deadlocked:/ ";
        for (size_t i = 0; i < deadlockedes.size(); i++) {
            cout << "P" << deadlockedes[i];
            if (i + 1 < deadlockedes.size()) {
                cout << " ";
            }
        }
        cout << "\n";
    }

    return 0;
}
//g++ -std=c++17 -O2 -o deadlock task_4.cpp