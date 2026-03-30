#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cout << "Usage: " << argv[0] << " <start_directory> <output_file> <bin_width>\n";
        return 1;
    }

    fs::path startPath = argv[1];
    string outputFile = argv[2];
    unsigned long long binWidth = stoull(argv[3]);

    if (binWidth == 0) {
        cout << "Bin width must be greater than 0.\n";
        return 1;
    }

    if (!fs::exists(startPath) || !fs::is_directory(startPath)) {
        cout << "Invalid directory.\n";
        return 1;
    }

    ofstream out(outputFile);
    if (!out) {
        cout << "Could not open output file.\n";
        return 1;
    }

    out << "BIN_WIDTH=" << binWidth << "\n";

    int totalFiles = 0;

    for (const auto& entry : fs::recursive_directory_iterator(startPath)) {
        try {
            if (fs::is_regular_file(entry.path())) {
                unsigned long long fileSize = fs::file_size(entry.path());
                unsigned long long binStart = (fileSize / binWidth) * binWidth;
                unsigned long long binEnd = binStart + binWidth - 1;

                out << binStart << "-" << binEnd << "\n";
                totalFiles++;
            }
        } catch (...) {
        }
    }

    cout << "Done. Files recorded: " << totalFiles << endl;
    cout << "Binned ranges written to: " << outputFile << endl;

    return 0;
}