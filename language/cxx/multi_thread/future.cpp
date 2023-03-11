#include <cassert>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static long long doLongTimeWork(int round, std::string tkName) {
    printf("%s at round %d start\n", tkName.c_str(), round);
    long long res = 0;
    for (int i = 0; i < round; i++) {
        res += i;
    }
    return res;
}

class AsyncTask {
public:
    void asyncTask() {
        int round = 10000000;
        std::future<long long> myFuture = std::async(doLongTimeWork, round, "Async");
        auto res = doLongTimeWork(round, "currentAsync");
        assert(res == myFuture.get());
    };
};

class PackagedTask {
public:
    void packagedTask() {
        int round = 10000000;
        // packaged_task本身并无异步功能，更多是为了包装！然后用于异步线程执行调用（例如线程池）
        std::packaged_task<long long(int, std::string)> ptk(doLongTimeWork);
        std::future<long long> myFuture = ptk.get_future();
        std::thread t(std::move(ptk), round, "Packaged");
        auto res = doLongTimeWork(round, "currentPackaged");
        t.join();
        assert(res == myFuture.get());
    }
};

class PromiseTask {
public:
    void promiseTask() {
        int round = 10000000;
        std::promise<long long> myPromise;
        std::future<long long> myFuture = myPromise.get_future();
        std::thread t([&]() {
            auto res = doLongTimeWork(round, "Promise");
            myPromise.set_value(res);
        });
        auto res = doLongTimeWork(round, "currentPromise");
        t.join();
        assert(res == myFuture.get());
    }
};

class SharedPromiseTask {
public:
    void sharedPromiseTask() {
        int round = 1000000000;
        std::promise<long long> myPromise;
        std::shared_future<long long> mySharedFuture = myPromise.get_future().share();
        // producer
        auto myProducer = [](std::promise<long long> p, int round) {
            auto res = doLongTimeWork(round, "SharedPromise");
            p.set_value(res);
        };
        std::thread t(myProducer, std::move(myPromise), round);

        // consumer
        int consumerCnt = 5;
        std::vector<std::thread> myThreads;
        std::vector<std::promise<long long>> myTotalPromises;
        std::vector<std::future<long long>> myTotalFutures;
        auto myConsumer = [](std::shared_future<long long> f, std::promise<long long> p) {
            long long res = f.get();
            p.set_value(res);
        };
        for (int i = 0; i < consumerCnt; i++) {
            std::promise<long long> p;
            myTotalFutures.emplace_back(std::move(p.get_future()));
            myTotalPromises.emplace_back(std::move(p));
        }
        for (int i = 0; i < consumerCnt; i++) {
            std::thread t(myConsumer, mySharedFuture, std::move(myTotalPromises[i]));
            myThreads.emplace_back(std::move(t));
        }

        auto res = doLongTimeWork(round, "currentSharedPromise");

        t.join();
        for (int i = 0; i < consumerCnt; i++) {
            myThreads[i].join();
        }

        for (int i = 0; i < consumerCnt; i++) {
            assert(res == myTotalFutures[i].get());
        }
    }
};


int main(int argc, char const* argv[]) {
    AsyncTask t;
    t.asyncTask();

    PackagedTask pckTk;
    pckTk.packagedTask();

    PromiseTask prTk;
    prTk.promiseTask();

    SharedPromiseTask sprTk;
    sprTk.sharedPromiseTask();

    return 0;
}
