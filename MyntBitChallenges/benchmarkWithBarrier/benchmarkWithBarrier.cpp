#include <vector>
#include <chrono>
#include <numeric>
#include <iostream>

using VectorDouble = std::vector<double>;
using Time = std::chrono::high_resolution_clock;

VectorDouble benchmarkWithBarrier(VectorDouble input) {
    //ensures memory ordering of t0 and t1 relative to the loop
    // ensures that the loop itself is not optimised by the compiler with volatile

    asm volatile("" ::: "memory");
    auto t0 = Time::now();
    asm volatile("" ::: "memory");
    volatile double sum_of_squares = std::accumulate(input.begin(), input.end(), 0.0,
        [](double acc, double val) {
            return acc + (val * val);
        }
    );
    asm volatile("" ::: "memory");
    auto t1 = Time::now();
    asm volatile("" ::: "memory");

    auto duration = t1 - t0;
    double timing_valid = (duration.count() >= 0) ? 1.0 : 0.0;

    return {timing_valid , sum_of_squares};
}

int main() {
    VectorDouble input = {1,2,3};
    VectorDouble res = benchmarkWithBarrier(input);
    for(const double& num : res) std::cout << num << std::endl;
    return 0;
}
