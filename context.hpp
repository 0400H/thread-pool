#pragma once

#ifndef __HPC_CONTEXT_HPP__
#define __HPC_CONTEXT_HPP__

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>

namespace hpc {

    template <typename T>
    class thread_context {
        public:
            std::vector<std::vector<std::vector<int>>> affinity_infos;
            std::condition_variable condition;
            std::queue<std::shared_ptr<T>> p_task;
            std::vector<std::shared_ptr<T>> c_task;
            std::mutex p_mt;
            std::mutex c_mt;
            std::mutex io_mt;
            bool verbose;
    };

}

#endif // __CONTEXT_HPP__
