#ifndef __CPU_HPP__
#define __CPU_HPP__

#include<cmath>
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
int get_cpu_sockets() {
    std::string cmd = "lscpu | grep -e 'Socket(s)' | awk  -F ':' '{print $2}' | awk '$1=$1'";
    FILE * fp = popen(cmd.c_str(),"r");
    char sockets[8];
    while (fgets(sockets, sizeof(sockets), fp) != NULL) {};
    fclose(fp);
    return std::stoi(sockets);
}

// https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
// https://stackoverflow.com/questions/18916254/get-the-current-cpuset
// https://stackoverflow.com/questions/280909/how-to-set-cpu-affinity-for-a-process-from-c-or-c-in-linux
std::vector<int> get_thread_affinity(bool verboe=false) {
    std::vector<int> cores;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (0 == sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset)) {
        const long nCores = sysconf( _SC_NPROCESSORS_ONLN );
        if (verboe) {
            std::cout << "affinity cores: ";
        }
        for (long i = 0; i < nCores; i++) {
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

std::size_t get_task_streams(int sockets, int streams=0) {
    return streams != 0 ? streams : sockets;
}

std::size_t get_task_concurrency(bool affinity=false) {
    if (affinity) {
        return get_thread_affinity(false).size();
    } else {
        return std::thread::hardware_concurrency();
    }
}

std::size_t get_task_threads(int threads) {
    if (threads < 0 || threads > MAX_THREADS) {
        throw std::exception();
    } else {
        return threads != 0 ? threads : get_task_concurrency();
    }
}

std::vector<std::vector<int>> get_thread_cores(int threads, int cores, std::vector<int>& task_cores) {
    std::vector<std::vector<int>> thread_cores(threads, std::vector<int>({}));

    if (threads <= cores) {
        int tail_cores_per_thread = cores / threads;
        int head_cores_per_thread = tail_cores_per_thread + 1;
        int head_thread_num = cores % threads;

        // std::cout << head_thread_num << ", " << head_cores_per_thread << ", " << tail_cores_per_thread << std::endl;

        int thread_idx = 0, core_idx = 0;
        for (; thread_idx < head_thread_num; thread_idx++) {
            auto core_stop = core_idx + head_cores_per_thread;
            for (; core_idx < core_stop; core_idx++) {
                thread_cores[thread_idx].emplace_back(core_idx);
            }
        }
        for (; thread_idx < threads; thread_idx++) {
            auto core_stop = core_idx + tail_cores_per_thread;
            for (; core_idx < core_stop; core_idx++) {
                thread_cores[thread_idx].emplace_back(core_idx);
            }
        }
    } else {
        int tail_thread_per_cores = threads / cores;
        int head_thread_per_cores = tail_thread_per_cores + 1;
        int head_thread_num = (threads % cores) * head_thread_per_cores;

        // std::cout << head_thread_num << ", " << head_thread_per_cores << ", " << tail_thread_per_cores << std::endl;

        int thread_idx = 0, core_idx = 0;
        for (; thread_idx < head_thread_num; thread_idx++) {
            thread_cores[thread_idx].emplace_back(core_idx);
            if (thread_idx % head_thread_per_cores == head_thread_per_cores-1) {
                core_idx++;
            };
        }
        for (; thread_idx < threads; thread_idx++) {
            thread_cores[thread_idx].emplace_back(core_idx);
            if ((thread_idx-head_thread_num) % tail_thread_per_cores == tail_thread_per_cores-1) {
                core_idx++;
            };
        }
    }

    for (auto i = 0; i < thread_cores.size(); i++) {
        for (auto j = 0; j < thread_cores[i].size(); j++) {
            thread_cores[i][j] = task_cores[thread_cores[i][j]];
        }
    }
    return thread_cores;
}

std::vector<std::vector<std::vector<int>>> get_stream_thread_affinity(int streams, int threads, bool verbose) {
    auto task_streams = get_task_streams(get_cpu_sockets(), streams);
    auto task_threads = get_task_threads(threads);
    auto task_cores = get_thread_affinity(verbose);
    auto task_cores_num = task_cores.size();

    std::vector<std::vector<int>> thread_cores = get_thread_cores(task_threads, task_cores_num, task_cores);

    int threads_per_stream = task_threads / task_streams;
    int threads_tail = task_threads % task_streams;

    std::vector<std::vector<std::vector<int>>> stream_thread_cores;

    auto thread_idx = 0;
    for (auto i = 0; i < task_streams; i++) {
        thread_idx = i * threads_per_stream;
        std::vector<std::vector<int>> tmp;
        for (; thread_idx < (i+1)*threads_per_stream; thread_idx++) {
            tmp.emplace_back(thread_cores[thread_idx]);
        }
        stream_thread_cores.emplace_back(tmp);
    }
    for (; thread_idx < task_threads; thread_idx++) {
        stream_thread_cores[task_streams-1].emplace_back(thread_cores[thread_idx]);
    }

    if (verbose) {
        for (auto a : stream_thread_cores) {
            std::cout << "stream affinity cores {";
            for (auto b : a) {
                std::cout << "[";
                for (auto c : b) {
                    std::cout << c << ",";
                }
                std::cout << "],";
            }
            std::cout << "}," << std::endl;
        }
    }

    return stream_thread_cores;
}

#endif // __CPU_HPP__
