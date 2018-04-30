#ifndef DLM_TIMER_HPP_
#define DLM_TIMER_HPP_
#include <chrono>
#include <functional>
#include <vector>
#include <algorithm>

// =================================================
//          Timer
// =================================================

class Timer
{
public:
    Timer() = default;
    explicit Timer(bool run) {
        if (run)
            start();
    }

    inline void start();
    inline void stop();
    inline void reset();
    inline double get_ms();

private:
    std::chrono::high_resolution_clock::time_point start_point;
    double dt = 0.0;
};

inline void Timer::start()
{
    start_point = std::chrono::high_resolution_clock::now();
}

inline void Timer::stop()
{
    get_ms();
}

inline void Timer::reset()
{
    dt = 0.0;
}

inline double Timer::get_ms()
{
    auto end_point = std::chrono::high_resolution_clock::now();
    dt += std::chrono::duration_cast<std::chrono::duration<double> >(end_point - start_point)
        .count();

    start_point = end_point;
    return dt;
}

#define TEST_CNT 15

class Benchmark {
    template <class Task>
    static double warmup(Task& task)
    {
        Timer timer(true);

        for (int i = 0; i < 5; ++i)
            task(timer);
    }

public:
    template <class Task>
    static double run(size_t maxItNum, double maxTime, Task task)
    {
        const int loc_it = 1;
        double dt = 0.0, last;
        size_t itCnt = 0;
        Timer t;
        std::vector<double> ts;
        ts.reserve(maxItNum / loc_it + 1);

        warmup(task);

        t.start();
        last = t.get_ms();
        while (itCnt < maxItNum && dt < maxTime)
        {

            for (int i=0; i<loc_it; ++i)
                task(t);

            auto nt = t.get_ms();
            dt = nt - last;
            last = nt;
            ts.push_back(dt / loc_it);
            itCnt += loc_it;
        }

        std::sort(ts.begin(), ts.end());
        return ts[ts.size() / 2] * 1000.0;
    }
};

#endif // DLM_TIMER_HPP_