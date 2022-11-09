#include <cassert>
#include <future>
#include <iostream>

class AsyncTask {
public:
    static long long doLongTimeWork(int round) {
        long long res = 0;
        for (int i = 0; i < round; i++) {
            res += i;
        }
        return res;
    };

    void asyncTask() {
        int round = 100000000;
        std::future<long long> myFuture = std::async(doLongTimeWork, round);
        auto res = doLongTimeWork(round);
        auto asyncRes = myFuture.get();
        assert(res == asyncRes);
        std::cout << "main res " << res << ", async res " << asyncRes << std::endl;
    };
};

int main(int argc, char const* argv[]) {
    AsyncTask t;
    t.asyncTask();
    return 0;
}
