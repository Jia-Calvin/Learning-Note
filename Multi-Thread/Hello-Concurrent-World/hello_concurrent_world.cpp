#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

void helloLisa() {
    std::cout << "HELLO LISA!" << std::endl;
}

int main(int argc, char const* argv[]) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; i++) {
        auto t = std::thread(helloLisa);
        threads.push_back(std::move(t));
    }

    for (int i = 0; i < threads.size(); i++) {
        threads[i].join();
    }

    return 0;
}