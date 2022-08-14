#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>

template <class T>
class ThreadSafeQueueWithLock {
private:
    std::mutex _mutex;
    std::condition_variable _cond;
    std::queue<std::shared_ptr<T>> _data;

public:
    ThreadSafeQueueWithLock(ThreadSafeQueueWithLock& other) = delete;
    ThreadSafeQueueWithLock& operator=(const ThreadSafeQueueWithLock& other) = delete;
    ThreadSafeQueueWithLock();
    ~ThreadSafeQueueWithLock();
    void push(T data);
    std::shared_ptr<T> wait_and_pop();
    void wait_and_pop(T& res);
    bool isEmpty();
};

template <class T>
ThreadSafeQueueWithLock<T>::ThreadSafeQueueWithLock() {}

template <class T>
ThreadSafeQueueWithLock<T>::~ThreadSafeQueueWithLock() {
    std::lock_guard<std::mutex> lk(_mutex);
    while (_data.empty()) {
        _data.pop();
    }
}

template <class T>
bool ThreadSafeQueueWithLock<T>::isEmpty() {
    std::lock_guard<std::mutex> lk(_mutex);
    return _data.empty();
}

template <class T>
void ThreadSafeQueueWithLock<T>::push(T input) {
    std::shared_ptr<T> data = std::make_shared<T>(std::move(input));
    std::lock_guard<std::mutex> lk(_mutex);
    _data.push(data);
    _cond.notify_one();
}

template <class T>
std::shared_ptr<T> ThreadSafeQueueWithLock<T>::wait_and_pop() {
    std::unique_lock<std::mutex> ul(_mutex);
    _cond.wait(ul, [&] { return !_data.empty(); });
    std::shared_ptr<T> res = _data.front();
    _data.pop();
    return res;
}

template <class T>
void ThreadSafeQueueWithLock<T>::wait_and_pop(T& res) {
    std::unique_lock<std::mutex> ul(_mutex);
    _cond.wait(ul, [&] { return !_data.empty(); });
    res = std::move(*_data.front());
    _data.pop();
}

int main(int argc, char const* argv[]) {
    ThreadSafeQueueWithLock<int> q;
    int a = 1;
    q.push(a);
    q.push(a);
    q.push(a);
    q.push(a);
    int res;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    q.wait_and_pop(res);
    std::cout << "res is " << res << std::endl;
    return 0;
}
