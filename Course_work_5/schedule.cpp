#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <fstream>
#include <string>

using namespace std;

struct Process {
    int id;
    int arrivalTime;
    int burstTime;
};

struct Segment {
    string algorithm;
    int processId;
    int startTime;
    int endTime;
};

bool compareArrival(const Process& a, const Process& b) {
    if (a.arrivalTime == b.arrivalTime) {
        return a.id < b.id;
    }
    return a.arrivalTime < b.arrivalTime;
}

double fcfs(vector<Process> processes, vector<Segment>& timeline) {
    sort(processes.begin(), processes.end(), compareArrival);

    int currentTime = 0;
    double totalWaitingTime = 0;

    for (const auto& p : processes) {
        if (currentTime < p.arrivalTime) {
            currentTime = p.arrivalTime;
        }

        int start = currentTime;
        int waitingTime = currentTime - p.arrivalTime;
        totalWaitingTime += waitingTime;

        currentTime += p.burstTime;
        int end = currentTime;

        timeline.push_back({"FCFS", p.id, start, end});
    }

    return totalWaitingTime / processes.size();
}

double sjf(vector<Process> processes, vector<Segment>& timeline) {
    int n = processes.size();
    vector<bool> completed(n, false);

    int currentTime = 0;
    int completedCount = 0;
    double totalWaitingTime = 0;

    while (completedCount < n) {
        int shortestIndex = -1;
        int shortestBurst = 1000000000;

        for (int i = 0; i < n; i++) {
            if (!completed[i] && processes[i].arrivalTime <= currentTime) {
                if (processes[i].burstTime < shortestBurst) {
                    shortestBurst = processes[i].burstTime;
                    shortestIndex = i;
                } else if (processes[i].burstTime == shortestBurst) {
                    if (shortestIndex == -1 || processes[i].arrivalTime < processes[shortestIndex].arrivalTime) {
                        shortestIndex = i;
                    }
                }
            }
        }

        if (shortestIndex == -1) {
            currentTime++;
            continue;
        }

        int start = currentTime;
        int waitingTime = currentTime - processes[shortestIndex].arrivalTime;
        totalWaitingTime += waitingTime;

        currentTime += processes[shortestIndex].burstTime;
        int end = currentTime;

        timeline.push_back({"SJF", processes[shortestIndex].id, start, end});

        completed[shortestIndex] = true;
        completedCount++;
    }

    return totalWaitingTime / n;
}

double roundRobin(vector<Process> processes, int quantum, vector<Segment>& timeline) {
    int n = processes.size();

    sort(processes.begin(), processes.end(), compareArrival);

    vector<int> remainingTime(n);
    vector<int> completionTime(n, 0);
    vector<bool> inQueue(n, false);

    for (int i = 0; i < n; i++) {
        remainingTime[i] = processes[i].burstTime;
    }

    queue<int> readyQueue;
    int currentTime = 0;
    int index = 0;
    int completedCount = 0;

    while (completedCount < n) {
        while (index < n && processes[index].arrivalTime <= currentTime) {
            if (!inQueue[index] && remainingTime[index] > 0) {
                readyQueue.push(index);
                inQueue[index] = true;
            }
            index++;
        }

        if (readyQueue.empty()) {
            if (index < n) {
                currentTime = processes[index].arrivalTime;
                continue;
            }
        } else {
            int i = readyQueue.front();
            readyQueue.pop();
            inQueue[i] = false;

            int start = currentTime;
            int timeSlice = min(quantum, remainingTime[i]);

            remainingTime[i] -= timeSlice;
            currentTime += timeSlice;
            int end = currentTime;

            timeline.push_back({"RoundRobin", processes[i].id, start, end});

            while (index < n && processes[index].arrivalTime <= currentTime) {
                if (!inQueue[index] && remainingTime[index] > 0) {
                    readyQueue.push(index);
                    inQueue[index] = true;
                }
                index++;
            }

            if (remainingTime[i] > 0) {
                readyQueue.push(i);
                inQueue[i] = true;
            } else {
                completionTime[i] = currentTime;
                completedCount++;
            }
        }
    }

    double totalWaitingTime = 0;

    for (int i = 0; i < n; i++) {
        int waitingTime = completionTime[i] - processes[i].arrivalTime - processes[i].burstTime;
        totalWaitingTime += waitingTime;
    }

    return totalWaitingTime / n;
}

int main() {
    int n;
    cout << "Enter number of processes: ";
    cin >> n;

    vector<Process> processes(n);

    cout << "\nEnter arrival time and burst time for each process:\n";
    for (int i = 0; i < n; i++) {
        processes[i].id = i + 1;

        cout << "Process P" << i + 1 << " arrival time: ";
        cin >> processes[i].arrivalTime;

        cout << "Process P" << i + 1 << " burst time: ";
        cin >> processes[i].burstTime;
    }

    int quantum;
    cout << "\nEnter time quantum for Round Robin: ";
    cin >> quantum;

    vector<Segment> fcfsTimeline;
    vector<Segment> sjfTimeline;
    vector<Segment> rrTimeline;

    double fcfsAvg = fcfs(processes, fcfsTimeline);
    double sjfAvg = sjf(processes, sjfTimeline);
    double rrAvg = roundRobin(processes, quantum, rrTimeline);

    cout << "\nAverage Waiting Times:\n";
    cout << "FCFS        : " << fcfsAvg << endl;
    cout << "SJF         : " << sjfAvg << endl;
    cout << "Round Robin : " << rrAvg << endl;

    ofstream resultsFile("results.txt");
    if (resultsFile.is_open()) {
        resultsFile << "Algorithm,AverageWaitingTime\n";
        resultsFile << "FCFS," << fcfsAvg << "\n";
        resultsFile << "SJF," << sjfAvg << "\n";
        resultsFile << "RoundRobin," << rrAvg << "\n";
        resultsFile.close();
        cout << "\nresults.txt created successfully.\n";
    } else {
        cout << "\nCould not create results.txt\n";
    }

    ofstream timelineFile("timeline.txt");
    if (timelineFile.is_open()) {
        timelineFile << "Algorithm,Process,Start,End\n";

        for (const auto& seg : fcfsTimeline) {
            timelineFile << seg.algorithm << ",P" << seg.processId << "," << seg.startTime << "," << seg.endTime << "\n";
        }

        for (const auto& seg : sjfTimeline) {
            timelineFile << seg.algorithm << ",P" << seg.processId << "," << seg.startTime << "," << seg.endTime << "\n";
        }

        for (const auto& seg : rrTimeline) {
            timelineFile << seg.algorithm << ",P" << seg.processId << "," << seg.startTime << "," << seg.endTime << "\n";
        }

        timelineFile.close();
        cout << "timeline.txt created successfully.\n";
    } else {
        cout << "Could not create timeline.txt\n";
    }

    return 0;
}