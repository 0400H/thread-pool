#pragma once

#ifndef __HPC_TASK_HPP__
#define __HPC_TASK_HPP__

#include <chrono>

namespace hpc {

class task_base {
    public:
        bool status;
        double task_time;

        task_base() {
            this->status = false;
        };

        virtual void process() = 0;

        virtual void run() final {
            auto start = std::chrono::system_clock::now();

            this->process();
            this->status = true;

            auto end = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            this->task_time = double(duration.count())
                            * std::chrono::microseconds::period::num
                            / std::chrono::microseconds::period::den
                            * 1000;
        };

        virtual bool wait(double wait_ms=0) final {
            if (wait_ms == 0) {
                while (!this->status) {};
                return true;
            } else {
                auto start = std::chrono::system_clock::now();
                while (!this->status) {
                    auto end = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                    auto delay_ms = double(duration.count())
                                  * std::chrono::microseconds::period::num
                                  / std::chrono::microseconds::period::den
                                  * 1000;
                    if (delay_ms >= wait_ms) {
                        return false;
                    }
                };
                return true;
            }
        };
};

}

#endif // __HPC_TASK_HPP__
