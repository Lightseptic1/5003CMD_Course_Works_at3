import csv
import matplotlib.pyplot as plt

algorithms = []
waiting_times = []

with open("results.txt", "r") as file:
    reader = csv.DictReader(file)
    for row in reader:
        algorithms.append(row["Algorithm"])
        waiting_times.append(float(row["AverageWaitingTime"]))

plt.figure(figsize=(8, 5))
bars = plt.bar(algorithms, waiting_times)
plt.title("Average Waiting Time by Scheduling Algorithm")
plt.xlabel("Algorithm")
plt.ylabel("Average Waiting Time")

for bar, value in zip(bars, waiting_times):
    plt.text(bar.get_x() + bar.get_width() / 2, bar.get_height(), f"{value:.2f}",
             ha='center', va='bottom')

plt.tight_layout()
plt.show()

timeline_data = []
with open("timeline.txt", "r") as file:
    reader = csv.DictReader(file)
    for row in reader:
        timeline_data.append({
            "algorithm": row["Algorithm"],
            "process": row["Process"],
            "start": int(row["Start"]),
            "end": int(row["End"])
        })

fig, ax = plt.subplots(figsize=(12, 6))

y_positions = {}
y = 0
for alg in ["FCFS", "SJF", "RoundRobin"]:
    processes_in_alg = sorted(set(d["process"] for d in timeline_data if d["algorithm"] == alg))
    for proc in processes_in_alg:
        y_positions[(alg, proc)] = y
        y += 1
    y += 1

for entry in timeline_data:
    duration = entry["end"] - entry["start"]
    y_pos = y_positions[(entry["algorithm"], entry["process"])]
    ax.barh(y_pos, duration, left=entry["start"], edgecolor="black")
    ax.text(entry["start"] + duration / 2, y_pos, entry["process"],
            ha='center', va='center')

yticks = []
yticklabels = []
for key, pos in y_positions.items():
    yticks.append(pos)
    yticklabels.append(f"{key[0]} - {key[1]}")

ax.set_yticks(yticks)
ax.set_yticklabels(yticklabels)
ax.set_xlabel("Time")
ax.set_title("CPU Scheduling Timeline (Gantt Chart)")
ax.grid(axis='x', linestyle='--', alpha=0.7)

plt.tight_layout()
plt.show()