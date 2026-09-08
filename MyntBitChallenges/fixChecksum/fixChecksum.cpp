#include <string>
#include <benchmark/benchmark.h>

class FIXValidator {
private:
public:
    bool isValid(const std::string& message) {
        //pointer which has access to the chars in message
        const char* p_start = message.data();

        //the last seven are always the checksum
        const char* p_end = p_start + message.size() - 7;

        //dereferences the pointer, then +1 for next round, till we reach end
        int total = 0;
        while(p_start < p_end) {
            total += static_cast<unsigned char>(*p_start++);
        }

        //manually finds the checksum at the end
        int provided = (p_start[3]-'0')*100 + (p_start[4]-'0')*10 + (p_start[5] - '0');
        return (total%256) == provided;


        // int total{0};
        // size_t len = message.size() - 7;
        // int checksum{0};
        // int expected_checksum{0};

        // for(int i{0}; i<(int)len ; ++i) {
        //     total += message[i];
        // }
        // int power{1};
        // for(int i = message.size()-2; i>message.size()-5; --i) {
        //     //std::cout << message[i] << std::endl;
        //     checksum += (message[i]-'0')*power;
        //     power *= 10;
        // }
        // //std::cout << total << std::endl;
        // expected_checksum = total % 256;
        // return expected_checksum == checksum;

    }
};

static void BM_FixValidator(benchmark::State& state) {
    FIXValidator validator;
    std::string msg = "8=FIX.4.2|9=5|35=0|10=018|";
    for (auto _ : state) {
        // DoNotOptimize prevents the compiler from optimizing the call away
        bool res = validator.isValid(msg);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_FixValidator);

BENCHMARK_MAIN();
