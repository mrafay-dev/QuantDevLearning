#include <condition_variable>
#include <mutex>

//unique_lock ensures compilation iwth cv.wait because
// thread can sleep whjile the mutex is unlocked

class CancellableQueue {
private:
    double queue[1024];
    int head {0};
    int tail {0};
    bool cancelled {false};
    std::mutex mtx;
    std::condition_variable cv;
public:
    CancellableQueue() {

    }

    void push(double val) {
        std::lock_guard<std::mutex> lck(mtx);
        if(!cancelled) {
            queue[tail++] = val;
        }
        cv.notify_one();
    }

    double pop() {
        //unique_lock because cv will only work with that
        std::unique_lock<std::mutex> lck(mtx);
        if(tail == head && !cancelled) cv.wait(lck);

        if(tail != head) return queue[head++];
        if(tail == head && cancelled) return -1;

        return -1;
    }

    void cancel() {
        std::lock_guard<std::mutex> lck(mtx);
        if(!cancelled){
            cancelled = true;
            cv.notify_all();
        }
    }
}
