import matplotlib.pyplot as plt

def read_ranges_file(file_path):
    histogram = {}
    bin_width = None

    with open(file_path, "r") as f:
        first_line = f.readline().strip()

        if first_line.startswith("BIN_WIDTH="):
            bin_width = int(first_line.split("=")[1])
        else:
            print("Error: first line must be BIN_WIDTH=number")
            return None, None

        for line in f:
            range_label = line.strip()
            if range_label:
                histogram[range_label] = histogram.get(range_label, 0) + 1

    return bin_width, histogram

def range_start(label):
    return int(label.split("-")[0])

def plot_histogram(bin_width, histogram):
    sorted_labels = sorted(histogram.keys(), key=range_start)
    counts = [histogram[label] for label in sorted_labels]

    plt.figure(figsize=(12, 6))
    plt.bar(sorted_labels, counts)

    plt.title(f"Histogram of File Sizes (Bin Width = {bin_width} bytes)")
    plt.xlabel("File Size Ranges (bytes)")
    plt.ylabel("Number of Files")

    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()

    plt.show()

def main():
    file_path = "ranges.txt"   # fixed file name

    bin_width, histogram = read_ranges_file(file_path)

    if histogram is None or not histogram:
        print("No valid data found.")
        return

    plot_histogram(bin_width, histogram)

if __name__ == "__main__":
    main()