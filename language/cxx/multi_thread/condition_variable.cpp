#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <unistd.h>
#include <vector>

std::mutex _m;
std::condition_variable _cond;
std::vector<int> _data;

void data_prepare_thread() {
    while (1) {
        {
            std::lock_guard<std::mutex> lk(_m);
            for (int i = 10; i < 99; i++) {
                _data.push_back(i);
            }
            _cond.notify_all();
        }
        sleep(1);
    }
}

void data_processing_thread() {
    while (1) {
        std::unique_lock<std::mutex> ul(_m);
        _cond.wait(ul, [&] { return !_data.empty(); });
        for (int i = 0; i < _data.size(); i++) {
            assert(_data[i] >= 10 && _data[i] < 99);
            std::cout << "processing ele: " << _data[i] << std::endl;
        }
        ul.unlock();
    }
}


int main(int argc, char const* argv[]) {
    std::cout << "start data_prepare_thread" << std::endl;
    auto pThread = std::thread(data_prepare_thread);
    std::cout << "start data_processing_thread" << std::endl;
    auto tThread = std::thread(data_processing_thread);
    pThread.join();
    tThread.join();
    return 0;
}
