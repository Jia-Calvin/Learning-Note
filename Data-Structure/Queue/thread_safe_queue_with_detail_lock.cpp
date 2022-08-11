#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

template <class T>
class ThreadSafeQueueWithDetailLock {
 public:
  struct Node {
    std::shared_ptr<T> _value;
    std::unique_ptr<Node> _next;
    Node(std::shared_ptr<T> value) : _value(std::move(value)) {}
    Node() {}
    ~Node() {}
  };

  ThreadSafeQueueWithDetailLock();
  ~ThreadSafeQueueWithDetailLock();
  void pushElement(T val);
  void popElement(T &val);

 private:
  std::mutex _headMutex;
  std::mutex _tailMutex;
  std::unique_ptr<Node> _head;
  Node *_tail;
  Node *_getTail() {
    std::lock_guard<std::mutex> lk(_tailMutex);
    return _tail;
  }
};

template <class T>
ThreadSafeQueueWithDetailLock<T>::ThreadSafeQueueWithDetailLock()
    : _head(std::make_unique<Node>(std::make_shared<T>())),
      _tail(_head.get()) {}

template <class T>
ThreadSafeQueueWithDetailLock<T>::~ThreadSafeQueueWithDetailLock() {}

template <class T>
void ThreadSafeQueueWithDetailLock<T>::pushElement(T val) {
  // before lock to allocate
  std::shared_ptr<T> newData = std::make_shared<T>(std::move(val));
  std::unique_ptr<Node> ptr = std::make_unique<Node>(std::move(newData));
  std::lock_guard<std::mutex> lk(_tailMutex);
  _tail->_next = std::move(ptr);
  _tail = _tail->_next.get();
}

template <class T>
void ThreadSafeQueueWithDetailLock<T>::popElement(T &val) {
  std::lock_guard<std::mutex> lk(_headMutex);
  if (_head.get() == _getTail()) {
    throw std::runtime_error("Empty Queue");
  }
  val = *_head->_next->_value;
  _head = std::move(_head->_next);
  return;
}

int main(int argc, char const *argv[]) {
  ThreadSafeQueueWithDetailLock<int> q;
  int threadCnt = 10;
  int insertNum = 100;
  std::vector<std::thread> ts;
  ts.reserve(threadCnt);
  for (int i = 0; i < threadCnt; i++) {
    std::thread t(
        [&](int threadId, int insertNum) {
          for (int i = threadId * insertNum;
               i < threadId * insertNum + insertNum; i++) {
            q.pushElement(i);
          }
        },
        i, insertNum);
    ts.push_back(std::move(t));
  }
  while (1) {
    try {
      int res;
      q.popElement(res);
      std::cout << "res is:" << res << std::endl;
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
      break;
    }
  }

  for (int i = 0; i < ts.size(); i++) {
    ts[i].join();
  }

  return 0;
}