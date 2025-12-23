#include "system_info.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <iomanip>
#include <algorithm>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <cstring>

#ifdef __linux__
#include <sys/sysinfo.h>
#ifdef HAS_LIBPCI
#include <pci/pci.h>
#endif
#endif

#ifdef __OpenBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/swap.h>
#endif

using namespace Utils;

void SystemInfo::detectUsername() {
    const char* user = std::getenv("USER");
    username = user ? user : "unknown";
}

void SystemInfo::detectHostname() {
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

void SystemInfo::detectOS() {
#ifdef __linux__
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
                if (os.front() == '"') os = os.substr(1);
                if (os.back() == '"') os.pop_back();
                break;
            } else if (line.find("NAME=") == 0 && os.empty()) {
                name = line.substr(5);
                if (name.front() == '"') name = name.substr(1);
                if (name.back() == '"') name.pop_back();
            }
        }
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

void SystemInfo::detectKernel() {
    struct utsname uts;
    if (uname(&uts) == 0) {
        kernel = uts.release;
        size_t dash = kernel.find('-');
        if (dash != std::string::npos) {
            kernel = kernel.substr(0, dash);
        }
    }
}

void SystemInfo::detectShell() {
    std::string shellPath = getEnv("SHELL");
    if (!shellPath.empty()) {
        size_t lastSlash = shellPath.find_last_of('/');
        shell = (lastSlash != std::string::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
    }
}

void SystemInfo::detectGTK() {
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

void SystemInfo::detectWM() {
    wm = getEnv("XDG_CURRENT_DESKTOP");
    if (wm.empty()) {
        wm = getEnv("DESKTOP_SESSION");
    }
    
#ifdef __linux__
    if (wm.empty()) {
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

void SystemInfo::detectUptime() {
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

void SystemInfo::detectMemory() {
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
    
    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM64;
    len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0) {
        long totalMB = physmem / (1024 * 1024);
        memory = std::to_string(totalMB) + "MB";
    }
    
    swap = "n/a";
#endif
}

void SystemInfo::detectCPU() {
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
                    
                    size_t at = cpu.find(" @");
                    if (at != std::string::npos) cpu = cpu.substr(0, at);
                    
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
    
    int maxCheck = coreCount > 0 ? coreCount + 2 : 32;
    for (int i = 0; i < maxCheck; i++) {
        std::string freqPath = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/cpuinfo_max_freq";
        std::ifstream freqFile(freqPath);
        if (freqFile.is_open()) {
            std::string freqStr;
            std::getline(freqFile, freqStr);
            if (!freqStr.empty()) {
                double freq = std::stod(freqStr) / 1000000.0;
                if (freq > maxFreq) {
                    maxFreq = freq;
                }
            }
            freqFile.close();
        }
    }
    
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

#ifdef __linux__
// Helper function to get cached Nix package count
static int getNixPackagesCached(const char* profilePath) {
    // Get cache directory
    const char* home = getenv("HOME");
    if (!home) return 0;
    
    std::string cacheDir = std::string(home) + "/.cache/motd";
    mkdir(cacheDir.c_str(), 0755);
    
    // Create cache filename based on profile path
    std::string cacheName = profilePath;
    std::replace(cacheName.begin(), cacheName.end(), '/', '_');
    std::string cacheFile = cacheDir + "/nix" + cacheName + ".cache";
    
    // Get current profile hash
    std::string hashCmd = "nix-store -q --hash " + std::string(profilePath) + " 2>/dev/null";
    FILE* hashPipe = popen(hashCmd.c_str(), "r");
    std::string currentHash;
    if (hashPipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), hashPipe) != nullptr) {
            currentHash = buffer;
            if (!currentHash.empty() && currentHash.back() == '\n') {
                currentHash.pop_back();
            }
        }
        pclose(hashPipe);
    }
    
    if (currentHash.empty()) return 0;
    
    // Try to read cache
    std::ifstream cache(cacheFile);
    if (cache.is_open()) {
        std::string cachedHash;
        int cachedCount;
        if (std::getline(cache, cachedHash) && cache >> cachedCount) {
            if (cachedHash == currentHash && cachedCount > 0) {
                cache.close();
                return cachedCount;
            }
        }
        cache.close();
    }
    
    // Cache miss or invalid - compute count
    std::string countCmd = 
        "nix-store -q --requisites " + std::string(profilePath) + " 2>/dev/null | "
        "grep -v '\\(-doc\\|-man\\|-info\\|-dev\\|-bin\\)$' | "
        "grep -v 'nixos-system-nixos-' | "
        "grep -E '[0-9]+\\.[0-9]+' | "
        "wc -l";
    
    FILE* countPipe = popen(countCmd.c_str(), "r");
    int count = 0;
    if (countPipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), countPipe) != nullptr) {
            count = std::atoi(buffer);
        }
        pclose(countPipe);
    }
    
    // Write to cache
    if (count > 0) {
        std::ofstream cacheOut(cacheFile);
        if (cacheOut.is_open()) {
            cacheOut << currentHash << "\n" << count;
            cacheOut.close();
        }
    }
    
    return count;
}
#endif

void SystemInfo::detectPackages() {
#ifdef __linux__
    // NixOS package detection with caching (matching fastfetch's approach)
    int nixSystemCount = 0;
    int nixUserCount = 0;
    bool isNixOS = false;
    
    // Check if /run/current-system exists
    struct stat st;
    if (stat("/run/current-system", &st) == 0) {
        nixSystemCount = getNixPackagesCached("/run/current-system");
        if (nixSystemCount > 0) {
            isNixOS = true;
        }
    }
    
    // Check for user packages
    const char* home = getenv("HOME");
    if (home) {
        std::string userProfile = std::string(home) + "/.nix-profile";
        if (stat(userProfile.c_str(), &st) == 0) {
            nixUserCount = getNixPackagesCached(userProfile.c_str());
            if (nixUserCount > 0) {
                isNixOS = true;
            }
        }
    }
    
    // Count Flatpak packages (system and user)
    int flatpakCount = 0;
    
    // System flatpak
    DIR* flatpakSystemDir = opendir("/var/lib/flatpak/app");
    if (flatpakSystemDir) {
        struct dirent* entry;
        while ((entry = readdir(flatpakSystemDir)) != nullptr) {
            if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                flatpakCount++;
            }
        }
        closedir(flatpakSystemDir);
    }
    
    // User flatpak
    if (home) {
        std::string userFlatpak = std::string(home) + "/.local/share/flatpak/app";
        DIR* flatpakUserDir = opendir(userFlatpak.c_str());
        if (flatpakUserDir) {
            struct dirent* entry;
            while ((entry = readdir(flatpakUserDir)) != nullptr) {
                if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                    flatpakCount++;
                }
            }
            closedir(flatpakUserDir);
        }
    }
    
    if (isNixOS) {
        if (nixSystemCount > 0 && nixUserCount > 0) {
            packages = std::to_string(nixSystemCount) + " (nix-system), " + 
                      std::to_string(nixUserCount) + " (nix-user)";
        } else if (nixSystemCount > 0) {
            packages = std::to_string(nixSystemCount) + " (nix-system)";
        } else {
            packages = std::to_string(nixUserCount) + " (nix-user)";
        }
        
        if (flatpakCount > 0) {
            packages += ", " + std::to_string(flatpakCount) + " (flatpak)";
        }
    }
    
    if (packages.empty()) {
        int apkCount = 0;
        std::ifstream apkFile("/lib/apk/db/installed");
        if (apkFile.is_open()) {
            std::string line;
            while (std::getline(apkFile, line)) {
                if (line.find("P:") == 0) {
                    apkCount++;
                }
            }
            apkFile.close();
            
            if (apkCount > 0) {
                packages = std::to_string(apkCount) + " (apk)";
            }
        }
    }
    
    if (packages.empty()) {
        int dpkgCount = 0;
        DIR* dpkgDir = opendir("/var/lib/dpkg/info");
        if (dpkgDir) {
            struct dirent* entry;
            while ((entry = readdir(dpkgDir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.length() > 5 && name.substr(name.length() - 5) == ".list") {
                    dpkgCount++;
                }
            }
            closedir(dpkgDir);
            
            if (dpkgCount > 0) {
                packages = std::to_string(dpkgCount) + " (dpkg)";
            }
        }
    }
    
    if (packages.empty()) {
        if (flatpakCount > 0) {
            packages = std::to_string(flatpakCount) + " (flatpak)";
        } else {
            packages = "0";
        }
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

void SystemInfo::detectTerminal() {
#ifdef __linux__
    pid_t ppid = getppid();
    
    for (int i = 0; i < 10; i++) {
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

void SystemInfo::detectModel() {
#ifdef __linux__
    model = trim(readFirstLine("/sys/devices/virtual/dmi/id/product_name"));
    if (model.find("System") == 0 || model.find("Default") == 0 || model.empty()) {
        model = trim(readFirstLine("/sys/devices/virtual/dmi/id/board_name"));
    }
#endif
}

void SystemInfo::detectLoadAvg() {
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

void SystemInfo::detectDisks() {
#ifdef __linux__
    std::ifstream file("/proc/mounts");
    if (file.is_open()) {
        std::string line;
        std::set<std::string> seen;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string device, mountpoint, fstype;
            iss >> device >> mountpoint >> fstype;
            
            if (mountpoint == "/" || mountpoint.find("/home") == 0 || 
                mountpoint.find("/mnt") == 0 || mountpoint.find("/media") == 0) {
                
                if (seen.find(mountpoint) != seen.end()) continue;
                seen.insert(mountpoint);
                
                struct statvfs stat;
                if (statvfs(mountpoint.c_str(), &stat) == 0) {
                    unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
                    unsigned long long free = (unsigned long long)stat.f_bfree * stat.f_frsize;
                    unsigned long long used = total - free;
                    
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
    FILE* pipe = popen("df -h / 2>/dev/null | tail -1", "r");
    if (pipe) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
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

void SystemInfo::detectGPU() {
#ifdef __linux__
#ifdef HAS_LIBPCI
    struct pci_access *pacc = pci_alloc();
    if (pacc) {
        pci_init(pacc);
        pci_scan_bus(pacc);
        
        for (struct pci_dev *dev = pacc->devices; dev; dev = dev->next) {
            pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
            
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
#else
    FILE* pipe = popen("lspci 2>/dev/null | grep -E 'VGA|3D|Display' | cut -d: -f3", "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string gpu = trim(buffer);
            if (!gpu.empty()) {
                gpus.push_back(gpu);
            }
        }
        pclose(pipe);
    }
#endif
#elif defined(__OpenBSD__)
    FILE* pipe = popen("dmesg 2>/dev/null | grep -E 'vga|radeon|intel|nvidia' | head -5", "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line = buffer;
            line = trim(line);
            if (!line.empty()) {
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

void SystemInfo::printColor(const std::string& label, const std::string& value, int color) {
    if (!value.empty()) {
        std::cout << "\033[9" << color << "m" << std::right << std::setw(8) 
                  << label << "\033[0m ~ " << value << std::endl;
    }
}

SystemInfo::SystemInfo() {
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

void SystemInfo::display() {
    std::cout << "\n" << username << "@" << hostname << "\n\n";
    
    printColor("os", os);
    printColor("sh", shell);
    printColor("wm", wm);
    printColor("up", uptime);
    printColor("gtk", gtk);
    printColor("box", model);
    printColor("cpu", cpu);
    
    for (const auto& gpu : gpus) {
        printColor("gpu", gpu);
    }
    
    printColor("mem", memory);
    printColor("swap", swap);
    
    for (const auto& disk : disks) {
        printColor("disk", disk);
    }
    
    printColor("load", loadAvg);
    printColor("kern", kernel);
    printColor("pkgs", packages);
    printColor("term", terminal);
    
    std::cout << "\n  ";
    for (int i = 0; i < 8; i++) {
        std::cout << "\033[3" << i << "m▅▅";
    }
    for (int i = 0; i < 8; i++) {
        std::cout << "\033[9" << i << "m▅▅";
    }
    std::cout << "\033[0m\n\n";
}