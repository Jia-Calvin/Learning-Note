#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// 执行命令：
// g++ thread_safe_stack_with_lock_free_reference.cpp -std=c++17 -o
// thread_safe_stack_with_lock_free_reference.exe &&
// ./thread_safe_stack_with_lock_free_reference.exe | wc -l


// C++ 语言由于没有自动的垃圾回收机制
// 因此在无锁数据结构里面，最关键的点就是需要考虑内存的回收
// 如果不需要顾及内存回收（允许内存泄漏），则无锁结构的实现是非常简单的
// 因此无锁数据结构，Go/JAVA的实现版本是很简单的，因为他们都带有自动回收垃圾的功能

// 以下的版本是使用 “引用计数” 的方法
// 进行垃圾回收的（只需要考虑Pop的并发，Push因为插入即完事，不需要考虑它）

template <class T>
class ThreadSafeStackWithLockFreeReference {
private:
    struct Node {
        std::shared_ptr<T> _val;
        Node* _next;
        Node(T const& val) : _val(std::make_shared<T>(val)) {}
    };
    std::atomic<Node*> _head;

    // 回收内存的相关隐私变量
    std::atomic<unsigned> _threadCntInPop;
    std::atomic<Node*> _needToBeDeletedNodes;

public:
    void push(T const& val);
    std::shared_ptr<T> pop();

    // 回收内存的相关方法
    void tryReclaim(Node* oldHead);
    void deleteNodes(Node* head) {
        while (head) {
            Node* next = head->_next;
            printf("delete node\n");
            delete head;
            head = next;
        }
    };
    void addInToDeletedNodesWithFirstAndLast(Node* first, Node* last) {
        assert(first);
        assert(last);
        last->_next = _needToBeDeletedNodes.load();
        while (!_needToBeDeletedNodes.compare_exchange_weak(last->_next, first))
            ;
    };
    void addLinkInToDeletedNodes(Node* head) {
        assert(head);
        Node* tail = head;
        while (tail->_next) {
            tail = tail->_next;
        }
        addInToDeletedNodesWithFirstAndLast(head, tail);
    }
    void addNodeInToDeletedNodes(Node* node) {
        assert(node);
        addInToDeletedNodesWithFirstAndLast(node, node);
    }
};

template <class T>
void ThreadSafeStackWithLockFreeReference<T>::push(T const& val) {
    // 先申请各种内存，异常的话，会直接抛出，链表结构不会受到破坏，因此异常安全
    Node* const newNode = new Node(val);

    // CAS 交换 head，避免 head 的条件竞争
    newNode->_next = _head.load();
    while (!_head.compare_exchange_weak(newNode->_next, newNode))
        ;
}

template <class T>
std::shared_ptr<T> ThreadSafeStackWithLockFreeReference<T>::pop() {
    // 先将计数引用加上，保证其他Pop线程感知到我的加入
    _threadCntInPop++;

    // CAS 交换 head，避免 head 的条件竞争
    Node* oldHead = _head.load();
    while (oldHead && !_head.compare_exchange_weak(oldHead, oldHead->_next))
        ;

    std::shared_ptr<T> res;
    if (oldHead) {
        // 直接将节点的数据全部转移给res，由调用者维护，res销毁，数据即销毁
        // 节点不再含有实际意义的数据了
        res.swap(oldHead->_val);
    }

    // 任意Pop都需要去判断是否能够回收内存中的节点（空数据，就单纯是节点）
    tryReclaim(oldHead);
    return res;
}

template <class T>
void ThreadSafeStackWithLockFreeReference<T>::tryReclaim(Node* oldHead) {
    if (_threadCntInPop.load() == 1) {
        // 可以尝试开始回收，目前只有自己在Pop中，先将 _needToBeDeletedNodes 收归己有
        Node* toBeDeletedNodesInMyThread = _needToBeDeletedNodes.exchange(nullptr);
        // 必须再次判断 “threadCntInPop” 是否只有自己在Pop，收归己有过程到这里有时间空隙
        if (!--_threadCntInPop) {
            // 真的只有自己一个线程了，可以回收所有的 “候删列表（needToBeDeletedNodes）”
            deleteNodes(toBeDeletedNodesInMyThread);
        } else if (toBeDeletedNodesInMyThread) {
            // 又线程线程在Pop了，必须把刚刚拿到的“候删列表（needToBeDeletedNodes）”重新加回队列去了
            addLinkInToDeletedNodes(toBeDeletedNodesInMyThread);
        }
        // 刚进来就判断到只有自己一个线程，因此oldHead完全可以直接删除掉
        // 原因是已经是老的head了，别的线程根本碰不到它（reclaim之前head已经更新过了）
        if (oldHead) {
            printf("delete node\n");
            delete oldHead;
        }
    } else {
        // 不可以回收，因为有并发Pop存在，等剩余一个Pop时再回收
        // 将oldHead加入到 “候删列表（needToBeDeletedNodes）” 中去
        if (oldHead) {
            addNodeInToDeletedNodes(oldHead);
        }
        --_threadCntInPop;
    }
}

int main(int argc, char const* argv[]) {
    ThreadSafeStackWithLockFreeReference<int> q;

    // push
    int threadCntPush = 100;
    int insertNum = 1000;
    std::vector<std::thread> tsPush;
    tsPush.reserve(threadCntPush);
    for (int i = 0; i < threadCntPush; i++) {
        std::thread t(
            [&q, insertNum](int threadId) {
                for (int j = threadId * insertNum; j < (threadId + 1) * insertNum; j++) {
                    q.push(j);
                }
            },
            i);
        tsPush.push_back(std::move(t));
    }

    // pop
    int threadCntPop = 100;
    std::vector<std::thread> tsPop;
    tsPop.reserve(threadCntPop);
    for (int i = 0; i < threadCntPop; i++) {
        std::thread t([&] {
            int emptyTime = 0;
            while (1) {
                try {
                    std::shared_ptr<int> res = q.pop();
                    if (res) {
                        printf("res is %d\n", *res);
                    } else {
                        emptyTime++;
                        if (emptyTime >= 100) {
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << e.what() << '\n';
                    break;
                }
            }
        });
        tsPop.push_back(std::move(t));
    }

    for (int i = 0; i < tsPush.size(); i++) {
        tsPush[i].join();
    }

    for (int i = 0; i < tsPop.size(); i++) {
        tsPop[i].join();
    }

    return 0;
}
