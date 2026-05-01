#include <fstream>
#include <string>
#include <iostream>

struct QualityMetrics {
    float hausdorff = 0.0f;
    float rms = 0.0f;
    bool success = false;
};

QualityMetrics parseMetroResults(const std::string& filename) {
    QualityMetrics metrics;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        return metrics;
    }

    while (std::getline(file, line)) {
        // Hausdorff line
        if (line.find("Hausdorff") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                metrics.hausdorff = std::stof(line.substr(pos + 1));
                metrics.success = true;
            }
        }

        // RMS line
        if (line.find("RMS") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                metrics.rms = std::stof(line.substr(pos + 1));
                metrics.success = true;
            }
        }
    }

    file.close();
    return metrics;
}
