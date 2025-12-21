#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>

namespace Utils {
    inline std::string getEnv(const char* name) {
        const char* val = std::getenv(name);
        return val ? val : "";
    }

    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    inline std::string formatSize(unsigned long long bytes) {
        double size = bytes;
        const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
        int unit = 0;
        
        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return oss.str();
    }

    inline std::string readFirstLine(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::string line;
        std::getline(file, line);
        return line;
    }
}