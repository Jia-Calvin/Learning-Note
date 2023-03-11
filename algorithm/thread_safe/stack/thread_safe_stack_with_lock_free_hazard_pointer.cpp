#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// C++ 语言由于没有自动的垃圾回收机制
// 因此在无锁数据结构里面，最关键的点就是需要考虑内存的回收
// 如果不需要顾及内存回收（允许内存泄漏），则无锁结构的实现是非常简单的
// 因此无锁数据结构，Go/JAVA的实现版本是很简单的，因为他们都带有自动回收垃圾的功能

// 以下的版本是使用 “风险指针” 的方法
// 进行垃圾回收的（只需要考虑Pop的并发，Push因为插入即完事，不需要考虑它）

class GlobalHazardPointer {
public:
    GlobalHazardPointer(){};
    ~GlobalHazardPointer(){};

    struct HazardPointer {
        std::atomic<std::thread::id> id;
        std::atomic<void*> pointer;
    };

    class HazardPointerHolder {
    private:
        HazardPointer* _hp;

    public:
        HazardPointerHolder(HazardPointer globalHazardPointer[]) {
            for (size_t i = 0; i < MaxHazardPointerSize; i++) {
                std::thread::id oldId;
                std::thread::id myThreadId = std::this_thread::get_id();
                if (globalHazardPointer[i].id.compare_exchange_strong(oldId, myThreadId)) {
                    _hp = &globalHazardPointer[i];
                    return;
                }
            }
            if (!_hp) {
                throw std::runtime_error("No Hazard pointers available");
            }
        };
        ~HazardPointerHolder(){};

        std::atomic<void*>& getPointer() {
            return _hp->pointer;
        }
    };

    std::atomic<void*>& getMyThreadHazardPointer() {
        static thread_local HazardPointerHolder hph(_globalHazardPointer);
        return hph.getPointer();
    }

    bool isStoreInGlobalHazardPointer(void* p) {
        for (size_t i = 0; i < MaxHazardPointerSize; i++) {
            std::thread::id myThreadId = std::this_thread::get_id();
            if (_globalHazardPointer[i].pointer.load() == p) {
                return true;
            }
        }
        return false;
    }

private:
    const static size_t MaxHazardPointerSize = 100;
    HazardPointer _globalHazardPointer[MaxHazardPointerSize];
};


template <class T>
class ThreadSafeStackWithLockFreeHazardPointer {
private:
    struct Node {
        std::shared_ptr<T> _val;
        Node* _next;
        Node(T const& val) : _val(std::make_shared<T>(val)) {}
    };
    std::atomic<Node*> _head;

    // 回收内存的相关隐私变量
    std::atomic<Node*> _needToBeDeletedNodes;
    std::unique_ptr<GlobalHazardPointer> _globalHazardPointer;

public:
    ThreadSafeStackWithLockFreeHazardPointer() {
        _globalHazardPointer = std::make_unique<GlobalHazardPointer>();
    }
    void push(T const& val);
    std::shared_ptr<T> pop();

    // 回收内存的相关方法
    void reclaimLater(Node* oldHead);
    void tryReclaimNodeWithNoHazardPointer();
    void addNodeInToDeletedNodes(Node* node) {
        node->_next = _needToBeDeletedNodes.load();
        while (!_needToBeDeletedNodes.compare_exchange_weak(node->_next, node))
            ;
    }
};

template <class T>
void ThreadSafeStackWithLockFreeHazardPointer<T>::push(T const& val) {
    // 先申请各种内存，异常的话，会直接抛出，链表结构不会受到破坏，因此异常安全
    Node* const newNode = new Node(val);

    // CAS 交换 head，避免 head 的条件竞争
    newNode->_next = _head.load();
    while (!_head.compare_exchange_weak(newNode->_next, newNode))
        ;
}

template <class T>
std::shared_ptr<T> ThreadSafeStackWithLockFreeHazardPointer<T>::pop() {
    // 获取当前线程对应的唯一风险指针，通过风险指针放置要访问的节点
    std::atomic<void*>& hp = _globalHazardPointer->getMyThreadHazardPointer();

    // CAS 交换 head，避免 head 的条件竞争
    // 必须通过双重循环的方式放置风险指针
    // 确保放置过程头节点与取出来的始终保持一致（没有被别的pop线程取出）
    Node *tmp, *oldHead;
    do {
        do {
            tmp = _head.load();
            hp.store(tmp);
            oldHead = _head.load();
            // FIXME(calvin): 这里还需要专门针对Node做new 和 delete 的实现，避免
            // oldHead 被清理了还能被访问，但是我们还拿着它的指针的值在访问，默认的new和delete这个行为是未定义的
            // 需要自己对节点的清理进行实现（new、delete）
        } while (oldHead != tmp);
    } while (oldHead && !_head.compare_exchange_weak(oldHead, oldHead->_next));

    // 当取出头节点后，自身的风险指针就可以设置为null了，后续线程不可能会再访问到它了
    hp.store(nullptr);

    std::shared_ptr<T> res;
    if (oldHead) {
        // 直接将节点的数据全部转移给res，由调用者维护，res销毁，数据即销毁
        // 节点不再含有实际意义的数据了
        res.swap(oldHead->_val);
        if (_globalHazardPointer->isStoreInGlobalHazardPointer(oldHead)) {
            // 同时有别的线程对它设置了风险指针，因此还不能够删除它！
            reclaimLater(oldHead);
        } else {
            // 所有运作中的线程都没不涉及访问到它了，因此可以直接删除了
            printf("delete node\n");
            delete oldHead;
        }
        // 尝试去回收没有风险指针的节点
        tryReclaimNodeWithNoHazardPointer();
    }

    return res;
}

template <class T>
void ThreadSafeStackWithLockFreeHazardPointer<T>::reclaimLater(Node* oldHead) {
    addNodeInToDeletedNodes(oldHead);
}

template <class T>
void ThreadSafeStackWithLockFreeHazardPointer<T>::tryReclaimNodeWithNoHazardPointer() {
    // 可以尝试开始回收，目前只有自己在Pop中，先将 _needToBeDeletedNodes 收归己有
    Node* currentNode = _needToBeDeletedNodes.exchange(nullptr);
    while (currentNode) {
        Node* nextNode = currentNode->_next;
        if (_globalHazardPointer->isStoreInGlobalHazardPointer(currentNode)) {
            addNodeInToDeletedNodes(currentNode);
        } else {
            printf("delete node\n");
            delete currentNode;
        }
        currentNode = nextNode;
    }
}

int main(int argc, char const* argv[]) {
    ThreadSafeStackWithLockFreeHazardPointer<int> q;

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
