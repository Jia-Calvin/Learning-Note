#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

int main(int argc, char const* argv[]) {
    std::mutex m1;
    std::mutex m2;
    std::unique_lock<std::mutex> lk1(m1, std::defer_lock);
    std::unique_lock<std::mutex> lk2(m2, std::defer_lock);

    {
        std::scoped_lock scopedLock(m1, m2);
        bool canLock1 = m1.try_lock();
        bool canLock2 = m2.try_lock();
        assert(!canLock1);
        assert(!canLock2);
    }

    lk2.lock();
    int canLock = std::try_lock(m1, m2);
    assert(canLock == 1);

    lk1.lock();
    canLock = std::try_lock(m1, m2);
    assert(canLock == 0);

    lk1.unlock();
    lk2.unlock();
    canLock = std::try_lock(m1, m2);
    assert(canLock == -1);

    return 0;
}
