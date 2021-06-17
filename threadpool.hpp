// https://zhuanlan.zhihu.com/p/340620340

#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__

#include "stream.hpp"
#include "context.hpp"

#include <iostream>
#include <vector>

template <typename T>
class thread_pool {
    public:
        thread_pool(int streams=0, int threads=0, bool verbose=false);
        ~thread_pool();

        std::shared_ptr<T> async(std::shared_ptr<T>);
        bool wait(std::shared_ptr<T>, double timeout=0);
        bool sync(std::shared_ptr<T>, bool direct=true);
        std::vector<std::shared_ptr<T>> wait_all();
        void clean_all();
        void reset_all();

    private:
        std::vector<std::shared_ptr<stream<T>>> streams;
        std::shared_ptr<thread_context<T>> contexts;
};

template <typename T>
thread_pool<T>::thread_pool(int streams, int threads, bool verbose) {
    this->contexts = std::make_shared<thread_context<T>>();
    this->contexts->verbose = verbose;
    this->contexts->affinity_infos = get_stream_thread_affinity(streams, threads, verbose);

    for (auto i = 0; i < this->contexts->affinity_infos.size(); i++) {
        this->streams.emplace_back(
            std::make_shared<stream<T>>(this->contexts, i)
        );
    }

}

template <typename T>
thread_pool<T>::~thread_pool() {
    this->clean_all();
}

template <typename T>
std::shared_ptr<T> thread_pool<T>::async(std::shared_ptr<T> task) {
    {
        std::lock_guard<std::mutex> p_lock(this->contexts->p_mt);
        this->contexts->p_task.push(task);
    }
    this->contexts->condition.notify_one();
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
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
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
        std::unique_lock<std::mutex> p_lock(this->contexts->p_mt);
        while (!this->contexts->p_task.empty()) {
            p_lock.unlock();
            p_lock.lock();
        };
    }

    return this->contexts->c_task;
}

template <typename T>
void thread_pool<T>::clean_all() {
    for (auto &stream : streams) {
        stream->terminate_threads();
    }

    this->contexts->p_task = std::queue<std::shared_ptr<T>>();
    std::vector<std::shared_ptr<T>>().swap(this->contexts->c_task);
}

template <typename T>
void thread_pool<T>::reset_all() {
    this->clean_all();
    for (auto &stream : streams) {
        stream->create_threads();
    }
}

#endif // __THREAD_POOL_HPP__
