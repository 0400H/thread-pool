#pragma once

#ifndef __HPC_AFFINITY_HPP__
#define __HPC_AFFINITY_HPP__

#include "cpu.hpp"

namespace hpc {

    std::vector<int> cal_parallel_cores(bool affinity=true) {
        std::vector<int> cores;
        if (affinity) {
            return get_thread_affinity(false);
        } else {
            auto hardware_concurrency = get_hardware_concurrency();
            for (auto i = 0; i < hardware_concurrency; i++) {
                cores.emplace_back(i);
            }
        }
        return cores;
    }

    std::size_t cal_thread_num(int thread_num, int core_num) {
        if (thread_num < 0 || thread_num > MAX_THREADS) {
            throw std::exception();
        } else if (thread_num == 0){
            return core_num;
        } else {
            return thread_num;
        }
    }

    std::size_t cal_stream_num(int stream_num) {
        return stream_num != 0 ? stream_num : get_hardware_sockets();
    }

    std::vector<std::vector<int>> cal_thread_affinity(
        int thread_num, std::vector<int>& cores) {
        std::vector<std::vector<int>> thread_affinity(thread_num, std::vector<int>({}));

        auto core_num = cores.size();
        if (thread_num <= core_num) {
            int tail_cores_per_thread = core_num / thread_num;
            int head_cores_per_thread = tail_cores_per_thread + 1;
            int head_thread_num = core_num % thread_num;

            // std::cout << head_thread_num << ", " << head_cores_per_thread << ", " << tail_cores_per_thread << std::endl;

            int thread_idx = 0, core_idx = 0;
            for (; thread_idx < head_thread_num; thread_idx++) {
                auto core_stop = core_idx + head_cores_per_thread;
                for (; core_idx < core_stop; core_idx++) {
                    thread_affinity[thread_idx].emplace_back(core_idx);
                }
            }
            for (; thread_idx < thread_num; thread_idx++) {
                auto core_stop = core_idx + tail_cores_per_thread;
                for (; core_idx < core_stop; core_idx++) {
                    thread_affinity[thread_idx].emplace_back(core_idx);
                }
            }
        } else {
            int tail_thread_per_cores = thread_num / core_num;
            int head_thread_per_cores = tail_thread_per_cores + 1;
            int head_thread_num = (thread_num % core_num) * head_thread_per_cores;

            // std::cout << head_thread_num << ", " << head_thread_per_cores << ", " << tail_thread_per_cores << std::endl;

            int thread_idx = 0, core_idx = 0;
            for (; thread_idx < head_thread_num; thread_idx++) {
                thread_affinity[thread_idx].emplace_back(core_idx);
                if (thread_idx % head_thread_per_cores == head_thread_per_cores-1) {
                    core_idx++;
                };
            }
            for (; thread_idx < thread_num; thread_idx++) {
                thread_affinity[thread_idx].emplace_back(core_idx);
                if ((thread_idx-head_thread_num) % tail_thread_per_cores == tail_thread_per_cores-1) {
                    core_idx++;
                };
            }
        }

        for (auto i = 0; i < thread_affinity.size(); i++) {
            for (auto j = 0; j < thread_affinity[i].size(); j++) {
                thread_affinity[i][j] = cores[thread_affinity[i][j]];
            }
        }
        return thread_affinity;
    }

    std::vector<std::vector<std::vector<int>>> cal_streams_affinity(
        int streams, int threads, bool affinity, bool verbose) {
        auto cores = cal_parallel_cores(affinity);
        auto cores_num = cores.size();
        auto thread_num = cal_thread_num(threads, cores_num);
        auto stream_num = cal_stream_num(streams);

        std::vector<std::vector<int>> thread_affinity =
            cal_thread_affinity(thread_num, cores);

        int threads_per_stream = thread_num / stream_num;
        int threads_tail = thread_num % stream_num;

        std::vector<std::vector<std::vector<int>>> streams_affinity;

        auto thread_idx = 0;
        for (auto i = 0; i < stream_num; i++) {
            thread_idx = i * threads_per_stream;
            std::vector<std::vector<int>> threads_affinity;
            for (; thread_idx < (i+1)*threads_per_stream; thread_idx++) {
                threads_affinity.emplace_back(thread_affinity[thread_idx]);
            }
            streams_affinity.emplace_back(threads_affinity);
        }

        for (; thread_idx < thread_num; thread_idx++) {
            streams_affinity[stream_num-1].emplace_back(thread_affinity[thread_idx]);
        }

        if (verbose) {
            for (auto stream : streams_affinity) {
                std::cout << "stream affinity cores {";
                for (auto thread : stream) {
                    std::cout << "[";
                    for (auto core : thread) {
                        std::cout << core << ",";
                    }
                    std::cout << "],";
                }
                std::cout << "}," << std::endl;
            }
        }

        return streams_affinity;
    }

}

#endif // __HPC_AFFINITY_HPP__