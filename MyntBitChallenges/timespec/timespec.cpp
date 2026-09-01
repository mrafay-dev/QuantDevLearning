//compile with g++ timespec.cpp -o timespec

#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <iostream>
#include <unistd.h> //for sleep
#include <chrono>
#include <thread>


using Time = struct timespec;

void sub_timespec(const Time* t1, const Time* t2, Time* delta) {
    std::cout << "t1-> sec: " << t1->tv_sec << "\n" << "t1-> nsec: " << t1->tv_nsec << std::endl;
    std::cout << "t2-> sec: " << t2->tv_sec << "\n" << "t2-> nsec: " << t2->tv_nsec << std::endl;
    delta->tv_sec = t2->tv_sec - t1->tv_sec;    //del second is t2 minus t1
    //1.8 3.1 : 3-1 = 2, 0.1-0.8
    delta->tv_nsec = t2->tv_nsec - t1->tv_nsec;
    if(delta->tv_nsec < 0) {
        delta->tv_nsec += 1000000000;
        --delta->tv_sec;
    }
}


int main() {

    Time start, end, delta;
    clock_gettime(CLOCK_REALTIME, &start);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    clock_gettime(CLOCK_REALTIME, &end);
    sub_timespec(&start, &end, &delta);
    std::cout << "Delta sec: " << delta.tv_sec << "\n" << "Delta nsec: " << delta.tv_nsec << std::endl;

}
