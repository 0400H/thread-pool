#pragma once

// https://zhuanlan.zhihu.com/p/340620340

#ifndef __HPC_THREAD_POOL_HPP__
#define __HPC_THREAD_POOL_HPP__

#include <iostream>
#include <vector>

#include "context.hpp"
#include "stream.hpp"

namespace hpc {

    template <typename T>
    class thread_pool {
        public:
            thread_pool(int stream_num=0, int thread_num=0,
                        bool affinity=true, bool verbose=false);
            ~thread_pool();

            std::shared_ptr<T> async(std::shared_ptr<T>);
            bool wait(std::shared_ptr<T>, double timeout=0);
            bool sync(std::shared_ptr<T>, bool direct=true);
            std::vector<std::shared_ptr<T>> wait_all();
            void clean_all();
            void reset_all();

        private:
            std::vector<std::shared_ptr<stream<T>>> streams;
            std::shared_ptr<thread_context<T>> context;
    };

    template <typename T>
    thread_pool<T>::thread_pool(int stream_num, int thread_num,
                                bool affinity, bool verbose) {
        this->context = std::make_shared<thread_context<T>>();
        this->context->verbose = verbose;
        this->streams = create_streams(this->context, stream_num, thread_num, affinity);
    }

    template <typename T>
    thread_pool<T>::~thread_pool() {
        this->clean_all();
    }

    template <typename T>
    std::shared_ptr<T> thread_pool<T>::async(std::shared_ptr<T> task) {
        {
            std::lock_guard<std::mutex> p_lock(this->context->p_mt);
            this->context->p_task.push(task);
        }
        this->context->condition.notify_one();
        return task;
    }

    template <typename T>
    bool thread_pool<T>::wait(std::shared_ptr<T> task, double timeout) {
        if (timeout == 0) {
            while (task->status != true) {};
            return true;
        } else {
            auto start = std::chrono::system_clock::now();
            double block_time = 0;
            while (task->status != true && block_time < timeout) {
                auto end = std::chrono::system_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                block_time = double(duration.count())
                            * std::chrono::microseconds::period::num
                            / std::chrono::microseconds::period::den
                            * 1000;
            };
            if (task->status) {
                return true;
            } else {
                return false;
            }
        }
    }

    template <typename T>
    bool thread_pool<T>::sync(std::shared_ptr<T> task, bool direct) {
        if (direct) {
            task->run();
            return true;
        } else {
            return this->wait(this->async(task));
        }
    }

    template <typename T>
    std::vector<std::shared_ptr<T>> thread_pool<T>::wait_all() {
        {
            std::unique_lock<std::mutex> p_lock(this->context->p_mt);
            while (!this->context->p_task.empty()) {
                p_lock.unlock();
                p_lock.lock();
            };
        }

        return this->context->c_task;
    }

    template <typename T>
    void thread_pool<T>::clean_all() {
        for (auto &stream : this->streams) {
            stream->clean_threads();
        }
        {
            std::lock_guard<std::mutex> p_lock(this->context->p_mt);
            this->context->p_task = std::queue<std::shared_ptr<T>>();
        }
        {
            std::lock_guard<std::mutex> c_lock(this->context->c_mt);
            std::vector<std::shared_ptr<T>>().swap(this->context->c_task);
        }
    }

    template <typename T>
    void thread_pool<T>::reset_all() {
        this->clean_all();
        for (auto &stream : this->streams) {
            stream->create_threads();
        }
    }

}

#endif // __HPC_THREAD_POOL_HPP__
