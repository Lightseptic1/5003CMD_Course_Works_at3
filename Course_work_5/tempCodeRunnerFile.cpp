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