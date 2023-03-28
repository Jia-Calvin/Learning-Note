#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>

template <class T>
class ThreadSafeQueueWithLock {
public:
    // constructor
    ThreadSafeQueueWithLock(ThreadSafeQueueWithLock& other) = delete;
    ThreadSafeQueueWithLock& operator=(const ThreadSafeQueueWithLock& other) = delete;
    ThreadSafeQueueWithLock();
    ~ThreadSafeQueueWithLock();

    // member function
    void push(T& data);
    std::shared_ptr<T> wait_and_pop();
    void wait_and_pop(T& res);
    std::shared_ptr<T> try_pop();
    bool try_pop(T& res);
    bool isEmpty();

private:
    mutable std::mutex _mutex;
    std::condition_variable _cond;
    std::queue<std::shared_ptr<T>> _data;
};

template <class T>
ThreadSafeQueueWithLock<T>::ThreadSafeQueueWithLock() {
    std::cout << "ThreadSafeQueueWithLock construct" << std::endl;
}

template <class T>
ThreadSafeQueueWithLock<T>::~ThreadSafeQueueWithLock() {
    std::lock_guard<std::mutex> lk(_mutex);
    std::cout << "ThreadSafeQueueWithLock release size:" << _data.size() << std::endl;
    while (!_data.empty()) {
        _data.pop();
    }
}

template <class T>
bool ThreadSafeQueueWithLock<T>::isEmpty() {
    std::lock_guard<std::mutex> lk(_mutex);
    return _data.empty();
}

template <class T>
void ThreadSafeQueueWithLock<T>::push(T& data) {
    std::cout << "---ready to push element---" << std::endl;
    // 这里使用std::move是避免调用拷贝构造函数对data进行多一次的拷贝
    // 但是我们的case里，我们的移动构造函数并没有真正的避免拷贝！！还是存在了string的拷贝！！
    // 所以使用std::move比较看重移动构造函数的实现，是否真正的避免的没有拷贝的情况！
    // 因此，在我们的case里，copy-construct和move-construct没有本质上的区别！
    std::shared_ptr<T> ptr = std::make_shared<T>(std::move(data));
    std::lock_guard<std::mutex> lk(_mutex);
    // 这里使用std::move是避免了引用计数的原子递增再原子递减
    // 直接就可以将所有权传递到_data里面，而无需变更引用计数
    _data.push(std::move(ptr));
    _cond.notify_all();
    std::cout << "---push element finished---" << std::endl;
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

template <class T>
std::shared_ptr<T> ThreadSafeQueueWithLock<T>::try_pop() {
    std::unique_lock<std::mutex> ul(_mutex);
    if (_data.empty()) {
        return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res = _data.front();
    _data.pop();
    return res;
}

template <class T>
bool ThreadSafeQueueWithLock<T>::try_pop(T& res) {
    std::unique_lock<std::mutex> ul(_mutex);
    if (_data.empty()) {
        return false;
    }
    res = std::move(*_data.front());
    _data.pop();
    return true;
}

class MyElement {
public:
    MyElement() {
        std::cout << "MyElement default-construct _a: " << _a << ", _a: " << &_a << std::endl;
    }
    explicit MyElement(std::string& a) {
        _a = a;
        std::cout << "MyElement explicit-construct _a: " << _a << ", _a: " << &_a << std::endl;
    }
    MyElement(const MyElement& other) {
        _a = std::move(other._a);
        std::cout << "MyElement copy-construct _a: " << _a << ", _a: " << &_a << std::endl;
    }
    MyElement(const MyElement&& other) {
        _a = std::move(other._a);
        std::cout << "MyElement move-construct _a: " << _a << ", _a: " << &_a << std::endl;
    }
    MyElement& operator=(const MyElement& other) {
        _a = other._a;
        std::cout << "MyElement assign-construct _a: " << _a << ", _a: " << &_a << std::endl;
        return *this;
    }
    ~MyElement() {
        std::cout << "MyElement release _a: " << _a << ", _a: " << &_a << std::endl;
    }
    std::string _a;
};


int main(int argc, char const* argv[]) {
    ThreadSafeQueueWithLock<MyElement> q;
    std::string a = "abc";
    MyElement* ele = new MyElement(a);
    q.push(*ele);
    q.push(*ele);
    q.push(*ele);
    q.push(*ele);

    // 外层的delete是无所谓的，q里面都是拷贝好的数据
    delete ele;
    ele = nullptr;

    MyElement res;

    q.wait_and_pop(res);
    std::cout << "res is " << res._a << std::endl;

    q.wait_and_pop(res);
    std::cout << "res is " << res._a << std::endl;

    q.wait_and_pop(res);
    std::cout << "res is " << res._a << std::endl;

    bool popSucc = q.try_pop(res);
    std::cout << "res is " << (popSucc ? res._a : "empty") << std::endl;

    popSucc = q.try_pop(res);
    std::cout << "res is " << (popSucc ? res._a : "empty") << std::endl;

    popSucc = q.try_pop(res);
    std::cout << "res is " << (popSucc ? res._a : "empty") << std::endl;

    return 0;
}
