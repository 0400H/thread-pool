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
            std::queue<std::shared_ptr<T>> queue;
            std::condition_variable condition;
            std::mutex mt;
            bool verbose;
    };

}

#endif // __CONTEXT_HPP__
