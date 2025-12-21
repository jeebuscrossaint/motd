#pragma once

#include <string>
#include <vector>

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

    void detectUsername();
    void detectHostname();
    void detectOS();
    void detectKernel();
    void detectShell();
    void detectGTK();
    void detectWM();
    void detectUptime();
    void detectMemory();
    void detectCPU();
    void detectPackages();
    void detectTerminal();
    void detectModel();
    void detectLoadAvg();
    void detectDisks();
    void detectGPU();
    
    void printColor(const std::string& label, const std::string& value, int color = 4);

public:
    SystemInfo();
    void display();
};