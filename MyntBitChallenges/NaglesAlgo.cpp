#include <vector>

class TcpSender {
    using DoubleVec = std::vector<double>;
private:
    bool nodelay_ {false};
    DoubleVec timestamps_;
    double time_{};
    double mss_{};
    double rtt_{};
    double buffer_{};
    double next_idle_time_{};
public:
    TcpSender(double mss, double rtt, bool nd)
        : mss_(mss), rtt_(rtt), nodelay_(nd) {}

    DoubleVec process(const DoubleVec& times,
                        const DoubleVec sizes){

        auto send = [&](double t) {
            double send_size = std::min(buffer_, mss_);
            timestamps_.push_back(t);
            buffer_ -= send_size;
            next_idle_time_ = t + rtt_;
        };

        for(size_t i = 0; i < times.size(); ++i) {
            double t = times[i];

            while(buffer_ > 0 && next_idle_time_ <= t) {
                send(next_idle_time_);
            }

            buffer_ += sizes[i];

            if(next_idle_time_ <= t) {
                send(t);
            }
        }

        while(buffer_>0) {
            send(next_idle_time_);
        }
        return timestamps_;
    }
};
