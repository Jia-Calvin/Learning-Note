#include <atomic>
#include <iostream>
#include <memory>

struct Node {
    std::string _val;
    std::atomic<Node*> _next;
    Node(const std::string& val, Node* next) : _val(val), _next(next) {}
};

class LockFreeQueue {
   public:
    LockFreeQueue();
    ~LockFreeQueue();

    void EnQueue(const std::string& val);
    std::string DeQueue();

   private:
    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;
};

LockFreeQueue::LockFreeQueue() {
    Node* dummy = new Node("", nullptr);
    _head.store(dummy);
    _tail.store(dummy);
}

void LockFreeQueue::EnQueue(const std::string& val) {
    Node* newNode = new Node(val, nullptr);
    Node *tail = nullptr, *tailNext = nullptr;
    while (1) {
        tail = _tail.load();

        tailNext = tail->_next.load();

        if (tail != _tail.load()) continue;

        if (tailNext != nullptr) {
            _tail.compare_exchange_strong(tail, tailNext);
            continue;
        }

        if (_tail.load()->_next.compare_exchange_strong(tailNext, newNode))
            break;
    }

    _tail.compare_exchange_strong(tail, newNode);
}

std::string LockFreeQueue::DeQueue() {
    std::string val;
    Node *head = nullptr, *headNext = nullptr, *tail = nullptr;
    while (1) {
        head = _head.load();
        headNext = head->_next.load();
        tail = _tail.load();

        if (head != _head.load()) continue;

        if (head == tail && headNext == nullptr) {
            std::cout << "Empty Queue, not element" << std::endl;
            throw "Empty Queue, not element";
        }

        if (head == tail && headNext != nullptr) {
            _tail.compare_exchange_strong(tail, headNext);
            continue;
        }

        if (_head.compare_exchange_strong(head, headNext)) {
            delete head;
            val = headNext->_val;
            break;
        }
    }

    return val;
}

int main(int argc, char const* argv[]) {
    LockFreeQueue lfq;
    lfq.EnQueue("1");
    lfq.EnQueue("2");
    lfq.EnQueue("3");
    lfq.EnQueue("4");
    lfq.EnQueue("5");
    lfq.EnQueue("6");

    std::cout << lfq.DeQueue() << std::endl;
    std::cout << lfq.DeQueue() << std::endl;
    std::cout << lfq.DeQueue() << std::endl;
    std::cout << lfq.DeQueue() << std::endl;
    std::cout << lfq.DeQueue() << std::endl;
    std::cout << lfq.DeQueue() << std::endl;

    return 0;
}
