#pragma once

#ifndef __HPC_CPU_HPP__
#define __HPC_CPU_HPP__

#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <bitset>
#include <sstream>
#include <iostream>
#include <thread>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace hpc {

    const int MAX_THREADS = 1000;

    #ifdef SYS_gettid
        #define gettid() ((pid_t)syscall(SYS_gettid))
        // pid_t gettid() {
        //     return syscall(SYS_gettid);
        // }
    #else
        #error "SYS_gettid unavailable on this system"
    #endif

    // http://www.cplusplus.com/forum/unices/6544/
    int get_hardware_sockets() {
        std::string cmd = "lscpu | grep -e 'Socket(s)' | awk  -F ':' '{print $2}' | awk '$1=$1'";
        FILE * fp = popen(cmd.c_str(),"r");
        char sockets[8];
        while (fgets(sockets, sizeof(sockets), fp) != NULL) {};
        fclose(fp);
        return std::stoi(sockets);
    }

    std::size_t get_hardware_concurrency() {
        return std::thread::hardware_concurrency();
    }

    // https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
    // https://stackoverflow.com/questions/18916254/get-the-current-cpuset
    // https://stackoverflow.com/questions/280909/how-to-set-cpu-affinity-for-a-process-from-c-or-c-in-linux
    std::vector<int> get_thread_affinity(bool verboe=false) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        std::vector<int> cores;
        if (0 == sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset)) {
            const int nCores = sysconf( _SC_NPROCESSORS_ONLN );
            if (verboe) {
                std::cout << "affinity cores: ";
            }
            for (int i = 0; i < nCores; i++) {
                if (CPU_ISSET(i, &cpuset)) {
                    cores.emplace_back(i);
                    if (verboe) {
                        std::cout << i << ",";
                    }
                }
            }
            if (verboe) {
                std::cout << std::endl;
            }
        } else {
            std::cerr << "sched_getaffinity() failed: " << strerror(errno) << std::endl;
        }
        return cores;
    }

    int set_thread_affinity(std::vector<int> &cores) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (auto core : cores) {
            CPU_SET(core, &mask);
        }
        return sched_setaffinity(0, sizeof(mask), &mask);
    }

}

#endif // __HPC_CPU_HPP__
