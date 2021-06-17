#ifndef __STREAM_HPP__
#define __STREAM_HPP__

#include "cpu.hpp"
#include "context.hpp"

#include <iostream>
#include <vector>

template <typename T>
class stream {
    public:
        stream(std::shared_ptr<thread_context<T>>, int);

        void create_threads();
        void terminate_threads();
        void reset_threads();

    private:
        static void worker(stream *, int);
        std::vector<std::thread> work_threads;
        std::shared_ptr<thread_context<T>> contexts;
        bool terminate;
        int thread_num;
        int id;
};

template <typename T>
stream<T>::stream(std::shared_ptr<thread_context<T>> contexts, int id) {
    this->contexts = contexts;
    this->id = id;

    this->create_threads();
};

template <typename T>
void stream<T>::create_threads() {
    this->terminate = false;
    this->thread_num = this->contexts->affinity_infos[this->id].size();
    for (int i = 0; i < thread_num; i++) {
        this->work_threads.emplace_back(this->worker, this, i);
        if (this->contexts->verbose) {
            std::cout << "stream:" << this->id << ", created thread:" << i << std::endl;
        }
    }
}

template <typename T>
void stream<T>::terminate_threads() {
    this->terminate = true;
    this->contexts->condition.notify_all();

    for (int i = 0; i < this->work_threads.size(); i++) {
        this->work_threads[i].join();
        if (this->contexts->verbose) {
            std::cout << "stream:" << this->id << ", joined thread:" << i << std::endl;
        }
    }
    this->work_threads.clear();
}

template <typename T>
void stream<T>::reset_threads() {
    this->terminate_threads();
    this->create_threads();
}

template <typename T>
void stream<T>::worker(stream *ptr, int i) {
    set_thread_affinity(ptr->contexts->affinity_infos[ptr->id][i]);

    while (!ptr->terminate) {
        std::unique_lock<std::mutex> p_lock(ptr->contexts->p_mt);
        ptr->contexts->condition.wait(p_lock, [ptr] {
            return (ptr->terminate || !ptr->contexts->p_task.empty());
        });

        if (ptr->terminate) {
            if (ptr->contexts->verbose) {
                std::cout << "stream:" << ptr->id << ", terminated worker:" << i << std::endl;
            }
            break;
        } else {
            auto task = ptr->contexts->p_task.front();
            ptr->contexts->p_task.pop();
            p_lock.unlock();
            task->run();
            {
                std::lock_guard<std::mutex> c_lock(ptr->contexts->c_mt);
                ptr->contexts->c_task.emplace_back(task);
            }
        }
    }
}

#endif // __STREAM_HPP__
