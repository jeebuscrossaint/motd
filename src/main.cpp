#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <iomanip>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <cstring>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <pci/pci.h>
#endif

#ifdef __OpenBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/swap.h>
#endif

class SystemInfo {
private:
    std::string username;
    std::string hostname;
    std::string os;
    std::string kernel;
    std::string shell;
    std::string wm;
    std::string uptime;
    std::string memory;
    std::string swap;
    std::string cpu;
    std::string packages;
    std::string terminal;
    std::string model;
    std::string gtk;
    std::vector<std::string> gpus;
    std::vector<std::string> disks;
    std::string loadAvg;

    std::string getEnv(const char* name) {
        const char* val = std::getenv(name);
        return val ? val : "";
    }

    std::string readFirstLine(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::string line;
        std::getline(file, line);
        return line;
    }

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    std::string formatSize(unsigned long long bytes) {
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

    void detectUsername() {
        const char* user = std::getenv("USER");
        username = user ? user : "unknown";
    }

    void detectHostname() {
        char buf[256];
        if (gethostname(buf, sizeof(buf)) == 0) {
            hostname = buf;
            size_t dot = hostname.find('.');
            if (dot != std::string::npos) {
                hostname = hostname.substr(0, dot);
            }
        } else {
            hostname = "unknown";
        }
    }

    void detectOS() {
#ifdef __linux__
        // Try /etc/os-release first
        std::ifstream file("/etc/os-release");
        if (!file.is_open()) {
            file.open("/usr/lib/os-release");
        }
        
        if (file.is_open()) {
            std::string line;
            std::string name;
            while (std::getline(file, line)) {
                if (line.find("PRETTY_NAME=") == 0) {
                    os = line.substr(12);
                    // Remove quotes
                    if (os.front() == '"') os = os.substr(1);
                    if (os.back() == '"') os.pop_back();
                    break;
                } else if (line.find("NAME=") == 0 && os.empty()) {
                    name = line.substr(5);
                    if (name.front() == '"') name = name.substr(1);
                    if (name.back() == '"') name.pop_back();
                }
            }
            // If we only got NAME (not PRETTY_NAME), append Linux for Gentoo
            if (os.empty() && !name.empty()) {
                os = name;
                if (os == "Gentoo" || os.find("Gentoo") != std::string::npos) {
                    if (os.find("Linux") == std::string::npos) {
                        os += " Linux";
                    }
                }
            }
        }
#elif defined(__OpenBSD__)
        std::ifstream dmesgFile("/var/run/dmesg.boot");
        if (dmesgFile.is_open()) {
            std::string line;
            while (std::getline(dmesgFile, line)) {
                if (line.find("OpenBSD") == 0) {
                    size_t space = line.find(' ', 8);
                    if (space != std::string::npos) {
                        os = line.substr(0, space);
                    } else {
                        os = line;
                    }
                    break;
                }
            }
        }
#endif
        if (os.empty()) {
            struct utsname uts;
            if (uname(&uts) == 0) {
                os = std::string(uts.sysname) + " " + uts.release;
            }
        }
    }

    void detectKernel() {
        struct utsname uts;
        if (uname(&uts) == 0) {
            kernel = uts.release;
            // Extract just the version number (before -)
            size_t dash = kernel.find('-');
            if (dash != std::string::npos) {
                kernel = kernel.substr(0, dash);
            }
        }
    }

    void detectShell() {
        std::string shellPath = getEnv("SHELL");
        if (!shellPath.empty()) {
            size_t lastSlash = shellPath.find_last_of('/');
            shell = (lastSlash != std::string::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
        }
    }

    void detectGTK() {
        std::string configPath = getEnv("XDG_CONFIG_HOME");
        if (configPath.empty()) {
            configPath = getEnv("HOME");
            if (!configPath.empty()) {
                configPath += "/.config";
            }
        }
        
        if (!configPath.empty()) {
            std::string settingsPath = configPath + "/gtk-3.0/settings.ini";
            std::ifstream file(settingsPath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (line.find("gtk-theme-name=") == 0 || line.find("gtk-theme=") == 0) {
                        size_t eq = line.find('=');
                        if (eq != std::string::npos) {
                            gtk = trim(line.substr(eq + 1));
                            break;
                        }
                    }
                }
            }
        }
    }

    void detectWM() {
        wm = getEnv("XDG_CURRENT_DESKTOP");
        if (wm.empty()) {
            wm = getEnv("DESKTOP_SESSION");
        }
        
#ifdef __linux__
        if (wm.empty()) {
            // Scan processes for common WMs
            DIR* dir = opendir("/proc");
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
                        std::string commPath = std::string("/proc/") + entry->d_name + "/comm";
                        std::string comm = trim(readFirstLine(commPath));
                        
                        if (comm.find("wm") != std::string::npos ||
                            comm == "i3" || comm == "sway" || comm == "dwm" ||
                            comm == "awesome" || comm == "xmonad" || 
                            comm.find("box") != std::string::npos) {
                            wm = comm;
                            break;
                        }
                    }
                }
                closedir(dir);
            }
        }
#endif
    }

    void detectUptime() {
#ifdef __linux__
        std::string uptimeStr = readFirstLine("/proc/uptime");
        if (!uptimeStr.empty()) {
            double uptimeSec = std::stod(uptimeStr);
            int totalSec = static_cast<int>(uptimeSec);
            int days = totalSec / 86400;
            int hours = (totalSec % 86400) / 3600;
            int mins = (totalSec % 3600) / 60;
            
            std::ostringstream oss;
            if (days > 0) oss << days << "d ";
            oss << hours << "h " << mins << "m";
            uptime = oss.str();
        }
#elif defined(__OpenBSD__)
        struct timespec ts;
        size_t len = sizeof(ts);
        int mib[2] = {CTL_KERN, KERN_BOOTTIME};
        
        if (sysctl(mib, 2, &ts, &len, NULL, 0) == 0) {
            time_t now = time(NULL);
            int totalSec = now - ts.tv_sec;
            int days = totalSec / 86400;
            int hours = (totalSec % 86400) / 3600;
            int mins = (totalSec % 3600) / 60;
            
            std::ostringstream oss;
            if (days > 0) oss << days << "d ";
            oss << hours << "h " << mins << "m";
            uptime = oss.str();
        }
#endif
    }

    void detectMemory() {
#ifdef __linux__
        std::ifstream file("/proc/meminfo");
        if (file.is_open()) {
            std::string line;
            unsigned long memTotal = 0, memAvail = 0, swapTotal = 0, swapFree = 0;
            
            while (std::getline(file, line)) {
                if (line.find("MemTotal:") == 0) {
                    sscanf(line.c_str(), "MemTotal: %lu", &memTotal);
                } else if (line.find("MemAvailable:") == 0) {
                    sscanf(line.c_str(), "MemAvailable: %lu", &memAvail);
                } else if (line.find("SwapTotal:") == 0) {
                    sscanf(line.c_str(), "SwapTotal: %lu", &swapTotal);
                } else if (line.find("SwapFree:") == 0) {
                    sscanf(line.c_str(), "SwapFree: %lu", &swapFree);
                }
            }
            
            unsigned long memUsed = memTotal - memAvail;
            int memPercent = memTotal > 0 ? (memUsed * 100) / memTotal : 0;
            
            std::ostringstream oss;
            oss << memUsed / 1024 << "MB / " << memTotal / 1024 << "MB (" << memPercent << "%)";
            memory = oss.str();
            
            if (swapTotal > 0) {
                unsigned long swapUsed = swapTotal - swapFree;
                int swapPercent = (swapUsed * 100) / swapTotal;
                std::ostringstream swapOss;
                swapOss << swapUsed / 1024 << "MB / " << swapTotal / 1024 << "MB (" << swapPercent << "%)";
                swap = swapOss.str();
            } else {
                swap = "none";
            }
        }
#elif defined(__OpenBSD__)
        int mib[2];
        int64_t physmem;
        size_t len;
        
        // Get total memory
        mib[0] = CTL_HW;
        mib[1] = HW_PHYSMEM64;
        len = sizeof(physmem);
        if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0) {
            // OpenBSD doesn't have an easy way to get available memory
            // This is a simplified version
            long totalMB = physmem / (1024 * 1024);
            memory = std::to_string(totalMB) + "MB";
        }
        
        // Swap info on OpenBSD is more complex, simplified here
        swap = "n/a";
#endif
    }

    void detectCPU() {
#ifdef __linux__
        std::ifstream file("/proc/cpuinfo");
        int coreCount = 0;
        double maxFreq = 0.0;
        
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("model name") == 0 && cpu.empty()) {
                    size_t colon = line.find(':');
                    if (colon != std::string::npos) {
                        cpu = trim(line.substr(colon + 1));
                        
                        // Clean up CPU string
                        size_t at = cpu.find(" @");
                        if (at != std::string::npos) cpu = cpu.substr(0, at);
                        
                        // Remove redundant words
                        size_t cpuWord = cpu.find(" CPU");
                        if (cpuWord != std::string::npos) cpu = cpu.substr(0, cpuWord);
                    }
                }
                if (line.find("processor") == 0) {
                    coreCount++;
                }
            }
            file.close();
        }
        
        // Get max CPU frequency - check all cores (for P-cores and E-cores)
        for (int i = 0; i < 100; i++) {
            std::string freqPath = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/cpuinfo_max_freq";
            std::ifstream freqFile(freqPath);
            if (freqFile.is_open()) {
                std::string freqStr;
                std::getline(freqFile, freqStr);
                if (!freqStr.empty()) {
                    double freq = std::stod(freqStr) / 1000000.0; // Convert KHz to GHz
                    if (freq > maxFreq) {
                        maxFreq = freq;
                    }
                }
            } else if (i > coreCount) {
                break; // No more CPUs to check
            }
        }
        
        // Fallback: try to get from /proc/cpuinfo if no frequency found
        if (maxFreq == 0.0) {
            std::ifstream cpuFile("/proc/cpuinfo");
            if (cpuFile.is_open()) {
                std::string line;
                while (std::getline(cpuFile, line)) {
                    if (line.find("cpu MHz") == 0) {
                        size_t colon = line.find(':');
                        if (colon != std::string::npos) {
                            double mhz = std::stod(trim(line.substr(colon + 1)));
                            if (mhz > maxFreq * 1000) {
                                maxFreq = mhz / 1000.0;
                            }
                        }
                    }
                }
            }
        }
        
        // Format CPU string with cores and frequency
        if (coreCount > 0 || maxFreq > 0) {
            std::ostringstream oss;
            oss << cpu;
            if (coreCount > 0) {
                oss << " (" << coreCount << ")";
            }
            if (maxFreq > 0) {
                oss << " @ " << std::fixed << std::setprecision(2) << maxFreq << " GHz";
            }
            cpu = oss.str();
        }
#elif defined(__OpenBSD__)
        char model[256];
        size_t len = sizeof(model);
        int mib[2] = {CTL_HW, HW_MODEL};
        
        if (sysctl(mib, 2, model, &len, NULL, 0) == 0) {
            cpu = model;
        }
#endif
    }

    void detectPackages() {
#ifdef __linux__
        // Gentoo - count category/package directories
        int gentooCount = 0;
        DIR* d = opendir("/var/db/pkg");
        if (d) {
            struct dirent* category;
            while ((category = readdir(d)) != nullptr) {
                if (category->d_name[0] == '.') continue;
                
                std::string categoryPath = std::string("/var/db/pkg/") + category->d_name;
                struct stat st;
                if (stat(categoryPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    DIR* catDir = opendir(categoryPath.c_str());
                    if (catDir) {
                        struct dirent* pkg;
                        while ((pkg = readdir(catDir)) != nullptr) {
                            if (pkg->d_name[0] != '.') gentooCount++;
                        }
                        closedir(catDir);
                    }
                }
            }
            closedir(d);
        }
        if (gentooCount > 0) {
            packages = std::to_string(gentooCount) + " (emerge)";
        }
        
        // Alpine Linux - count apk packages
        if (packages.empty()) {
            int apkCount = 0;
            DIR* apkDir = opendir("/lib/apk/db/installed");
            if (apkDir) {
                struct dirent* entry;
                while ((entry = readdir(apkDir)) != nullptr) {
                    if (entry->d_name[0] != '.' && entry->d_type == DT_REG) {
                        apkCount++;
                    }
                }
                closedir(apkDir);
                
                if (apkCount > 0) {
                    packages = std::to_string(apkCount) + " (apk)";
                }
            }
        }
        
        if (packages.empty()) {
            packages = "0";
        }
#elif defined(__OpenBSD__)
        int obsdCount = 0;
        DIR* d = opendir("/var/db/pkg");
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                if (entry->d_name[0] != '.') obsdCount++;
            }
            closedir(d);
        }
        if (obsdCount > 0) {
            packages = std::to_string(obsdCount) + " (pkg)";
        } else {
            packages = "0";
        }
#endif
    }

    void detectTerminal() {
#ifdef __linux__
        pid_t ppid = getppid();
        
        for (int i = 0; i < 10; i++) {  // Limit iterations
            std::string statusPath = "/proc/" + std::to_string(ppid) + "/status";
            std::ifstream file(statusPath);
            if (!file.is_open()) break;
            
            std::string line;
            pid_t nextPpid = 0;
            while (std::getline(file, line)) {
                if (line.find("PPid:") == 0) {
                    sscanf(line.c_str(), "PPid: %d", &nextPpid);
                    break;
                }
            }
            file.close();
            
            if (nextPpid == 0) break;
            
            std::string commPath = "/proc/" + std::to_string(ppid) + "/comm";
            std::string comm = trim(readFirstLine(commPath));
            
            // Skip shells, init, and build tools
            if (comm.find("sh") == std::string::npos &&
                comm.find("login") == std::string::npos &&
                comm.find("init") == std::string::npos &&
                comm.find("systemd") == std::string::npos &&
                comm != "xmake" &&
                comm != "make" &&
                comm != "cmake") {
                terminal = comm;
                break;
            }
            
            ppid = nextPpid;
        }
#endif
        if (terminal.empty()) terminal = getEnv("TERM");
    }

    void detectModel() {
#ifdef __linux__
        model = trim(readFirstLine("/sys/devices/virtual/dmi/id/product_name"));
        if (model.find("System") == 0 || model.find("Default") == 0 || model.empty()) {
            model = trim(readFirstLine("/sys/devices/virtual/dmi/id/board_name"));
        }
#endif
    }

    void detectLoadAvg() {
#ifdef __linux__
        std::ifstream file("/proc/loadavg");
        if (file.is_open()) {
            double load1, load5, load15;
            if (file >> load1 >> load5 >> load15) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << load1 << ", " 
                    << load5 << ", " << load15;
                loadAvg = oss.str();
            }
        }
#elif defined(__OpenBSD__)
        double loads[3];
        if (getloadavg(loads, 3) != -1) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << loads[0] << ", " 
                << loads[1] << ", " << loads[2];
            loadAvg = oss.str();
        }
#endif
    }

    void detectDisks() {
#ifdef __linux__
        std::ifstream file("/proc/mounts");
        if (file.is_open()) {
            std::string line;
            std::set<std::string> seen;
            
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string device, mountpoint, fstype;
                iss >> device >> mountpoint >> fstype;
                
                // Skip non-physical filesystems and duplicates
                if (mountpoint == "/" || mountpoint.find("/home") == 0 || 
                    mountpoint.find("/mnt") == 0 || mountpoint.find("/media") == 0) {
                    
                    if (seen.find(mountpoint) != seen.end()) continue;
                    seen.insert(mountpoint);
                    
                    struct statvfs stat;
                    if (statvfs(mountpoint.c_str(), &stat) == 0) {
                        unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
                        unsigned long long avail = (unsigned long long)stat.f_bavail * stat.f_frsize;
                        unsigned long long used = total - avail;
                        
                        if (total > 0) {
                            int percent = (used * 100) / total;
                            std::ostringstream oss;
                            oss << mountpoint << ": " << formatSize(used) << " / " 
                                << formatSize(total) << " (" << percent << "%) - " << fstype;
                            disks.push_back(oss.str());
                        }
                    }
                }
            }
        }
#elif defined(__OpenBSD__)
        // OpenBSD disk detection using getmntinfo
        FILE* pipe = popen("df -h / 2>/dev/null | tail -1", "r");
        if (pipe) {
            char buffer[512];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                // Parse df output: Filesystem Size Used Avail Capacity Mounted
                std::string line = buffer;
                std::istringstream iss(line);
                std::string fs, size, used, avail, capacity;
                iss >> fs >> size >> used >> avail >> capacity;
                
                if (!capacity.empty()) {
                    disks.push_back("/: " + used + " / " + size + " (" + capacity + ")");
                }
            }
            pclose(pipe);
        }
#endif
    }

    void detectGPU() {
#ifdef __linux__
        // Use libpci directly for fast GPU detection
        struct pci_access *pacc = pci_alloc();
        if (pacc) {
            pci_init(pacc);
            pci_scan_bus(pacc);
            
            for (struct pci_dev *dev = pacc->devices; dev; dev = dev->next) {
                pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
                
                // Check if it's a VGA/Display controller (class 0x03)
                if ((dev->device_class >> 8) == 0x03) {
                    char namebuf[512];
                    char *name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
                                                   PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
                                                   dev->vendor_id, dev->device_id);
                    if (name) {
                        gpus.push_back(std::string(name));
                    }
                }
            }
            
            pci_cleanup(pacc);
        }
#elif defined(__OpenBSD__)
        // On OpenBSD, parse dmesg output for GPU info
        FILE* pipe = popen("dmesg 2>/dev/null | grep -E 'vga|radeon|intel|nvidia' | head -5", "r");
        if (pipe) {
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string line = buffer;
                line = trim(line);
                if (!line.empty()) {
                    // Extract GPU info from dmesg line
                    size_t colon = line.find(':');
                    if (colon != std::string::npos && colon + 2 < line.length()) {
                        gpus.push_back(trim(line.substr(colon + 1)));
                    }
                }
            }
            pclose(pipe);
        }
#endif
    }

    void printColor(const std::string& label, const std::string& value, int color = 4) {
        if (!value.empty()) {
            std::cout << "\033[9" << color << "m" << std::right << std::setw(8) 
                      << label << "\033[0m ~ " << value << std::endl;
        }
    }

public:
    SystemInfo() {
        detectUsername();
        detectHostname();
        detectOS();
        detectKernel();
        detectShell();
        detectWM();
        detectUptime();
        detectMemory();
        detectCPU();
        detectPackages();
        detectTerminal();
        detectModel();
        detectGTK();
        detectGPU();
        detectLoadAvg();
        detectDisks();
    }

    void display() {
        // User@Host header
        std::cout << "\n" << username << "@" << hostname << "\n\n";
        
        // System info - ordered: os, sh, wm, up, gtk, box, cpu, gpu, mem, swap, kern, pkgs, term
        printColor("os", os);
        printColor("sh", shell);
        printColor("wm", wm);
        printColor("up", uptime);
        printColor("gtk", gtk);
        printColor("box", model);
        printColor("cpu", cpu);
        
        // Print GPUs
        for (const auto& gpu : gpus) {
            printColor("gpu", gpu);
        }
        
        printColor("mem", memory);
        printColor("swap", swap);
        
        // Print disks
        for (const auto& disk : disks) {
            printColor("disk", disk);
        }
        
        printColor("load", loadAvg);
        printColor("kern", kernel);
        printColor("pkgs", packages);
        printColor("term", terminal);
        
        // Color blocks - all 16 colors in one line
        std::cout << "\n  ";
        // Normal colors (30-37)
        for (int i = 0; i < 8; i++) {
            std::cout << "\033[3" << i << "m▅▅";
        }
        // Bright colors (90-97)
        for (int i = 0; i < 8; i++) {
            std::cout << "\033[9" << i << "m▅▅";
        }
        std::cout << "\033[0m\n\n";
    }
};

int main() {
    SystemInfo info;
    info.display();
    return 0;
}