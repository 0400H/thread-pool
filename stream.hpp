#pragma once

#ifndef __HPC_STREAM_HPP__
#define __HPC_STREAM_HPP__

#include <iostream>
#include <vector>

#include "context.hpp"
#include "affinity.hpp"
#include "cpu.hpp"

namespace hpc {

    template <typename T>
    class stream {
        public:
            stream(std::shared_ptr<thread_context<T>> context, int id);

            void create_threads();
            void clean_threads();
            void reset_threads();

        private:
            static void worker(stream *, int);
            std::vector<std::thread> work_threads;
            std::shared_ptr<thread_context<T>> context;
            bool terminate;
            int thread_num;
            int id;
    };

    template <typename T>
    stream<T>::stream(std::shared_ptr<thread_context<T>> context, int id) {
        this->context = context;
        this->id = id;
        this->create_threads();
    };

    template <typename T>
    void stream<T>::create_threads() {
        this->terminate = false;
        this->thread_num = this->context->affinity_infos[this->id].size();
        for (int i = 0; i < thread_num; i++) {
            this->work_threads.emplace_back(this->worker, this, i);
            if (this->context->verbose) {
                std::cout << "stream:" << this->id << ", created thread:" << i << std::endl;
            }
        }
    }

    template <typename T>
    void stream<T>::clean_threads() {
        this->terminate = true;
        this->context->condition.notify_all();

        for (int i = 0; i < this->work_threads.size(); i++) {
            this->work_threads[i].join();
            if (this->context->verbose) {
                std::cout << "stream:" << this->id << ", joined thread:" << i << std::endl;
            }
        }
        std::vector<std::thread> ().swap(this->work_threads);
    }

    template <typename T>
    void stream<T>::reset_threads() {
        this->clean_threads();
        this->create_threads();
    }

    template <typename T>
    void stream<T>::worker(stream *ptr, int i) {
        set_thread_affinity(ptr->context->affinity_infos[ptr->id][i]);

        while (!ptr->terminate) {
            std::unique_lock<std::mutex> p_lock(ptr->context->p_mt);
            ptr->context->condition.wait(p_lock, [ptr] {
                return (ptr->terminate || !ptr->context->p_task.empty());
            });

            if (ptr->terminate) {
                if (ptr->context->verbose) {
                    std::cout << "stream:" << ptr->id << ", terminated worker:" << i << std::endl;
                }
                break;
            } else {
                auto task = ptr->context->p_task.front();
                ptr->context->p_task.pop();
                p_lock.unlock();
                task->run();
                {
                    std::lock_guard<std::mutex> c_lock(ptr->context->c_mt);
                    ptr->context->c_task.emplace_back(task);
                }
            }
        }
    }

    template <typename T>
    std::vector<std::shared_ptr<stream<T>>> create_streams(
        std::shared_ptr<thread_context<T>> context,
        int stream_num, int thread_num, bool affinity) {
        context->affinity_infos = cal_streams_affinity(
            stream_num, thread_num, affinity, context->verbose);

        std::vector<std::shared_ptr<stream<T>>> streams;
        for (auto i = 0; i < context->affinity_infos.size(); i++) {
            streams.emplace_back(
                std::make_shared<stream<T>>(context, i)
            );
        }
        return streams;
    }

}

#endif // __HPC_STREAM_HPP__
