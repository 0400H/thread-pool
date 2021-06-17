#ifndef __TASK_HPP__
#define __TASK_HPP__

#include <chrono>

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
};

#endif // __TASK_HPP__
