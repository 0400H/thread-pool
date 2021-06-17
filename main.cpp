#include "threadpool.hpp"

#include <chrono>

class task {
    private:
        int block_ms;
        bool verbose;

    public:
        int task_id;
        bool status;

        task(int block_ms, int task_id, bool verbose) {
            this->block_ms = block_ms;
            this->task_id = task_id;
            this->verbose = verbose;
            this->status = false;
        };

        void process() {
            auto start = std::chrono::system_clock::now();
            while (true) {
                auto end = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                auto block_time = double(duration.count())
                                * std::chrono::microseconds::period::num
                                / std::chrono::microseconds::period::den
                                * 1000;
                if (block_time > block_ms) {
                    if (this->verbose) {
                        std::cout << "task_id:" << this->task_id
                                  << ", pid:" << getpid()
                                  << ", tid:" << gettid()
                                  << ", tidx:" << gettid() - getpid()
                                  << ", time:" << block_time << "ms"
                                  << std::endl;
                    }
                    break;
                }
            }
            this->status = true;
        }

        int get() {
            return this->block_ms;
        }
};

int main() {
    int loop = 1000;
    bool verbose = false;

    class thread_pool<task> tp(0, 0, verbose);

    // sync direct
    {
        tp.reset_all();

        auto start = std::chrono::system_clock::now();

        auto id = loop;
        while (id--) {
            tp.sync(std::make_shared<task>(10, id, verbose), true);
        }

        auto end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto block_time = double(duration.count())
                        * std::chrono::microseconds::period::num
                        / std::chrono::microseconds::period::den;
        std::cout << "Sync API(direct) Summary"
                  << ", Whole Time: " << block_time << "s"
                  << ", Avg Latency: " << block_time / loop * 1000 << "ms"
                  << ", Avg QPS: " << loop / block_time
                  << std::endl;
    }

    // sync use async api
    {
        tp.reset_all();

        auto start = std::chrono::system_clock::now();

        auto id = loop;
        while (id--) {
            tp.sync(std::make_shared<task>(10, id, verbose), false);
        }

        auto end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto block_time = double(duration.count())
                        * std::chrono::microseconds::period::num
                        / std::chrono::microseconds::period::den;
        std::cout << "Sync API(async) Summary"
                  << ", Whole Time: " << block_time << "s"
                  << ", Avg Latency: " << block_time / loop * 1000 << "ms"
                  << ", Avg QPS: " << loop / block_time
                  << std::endl;
    }

    // async api
    {
        tp.reset_all();

        auto start = std::chrono::system_clock::now();

        auto id = loop * std::thread::hardware_concurrency();
        while (id--) {
            tp.async(std::make_shared<task>(10, id, verbose));
        }
        tp.wait_all();

        auto end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto block_time = double(duration.count())
                        * std::chrono::microseconds::period::num
                        / std::chrono::microseconds::period::den;
        std::cout << "Async API Summary"
                  << ", Whole Time: " << block_time << "s"
                  << ", Avg Latency: " << block_time / loop * 1000 << "ms"
                  << ", Avg QPS: " << loop * std::thread::hardware_concurrency() / block_time
                  << std::endl;
    }

    return 0;
}