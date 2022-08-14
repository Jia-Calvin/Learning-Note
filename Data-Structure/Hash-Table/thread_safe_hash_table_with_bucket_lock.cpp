#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

template <typename K, typename V, typename Hash = std::hash<K>>
class ThreadSafeHashTableWithBucketLock {
public:
    ThreadSafeHashTableWithBucketLock(const ThreadSafeHashTableWithBucketLock& other) = delete;
    ThreadSafeHashTableWithBucketLock& operator=(const ThreadSafeHashTableWithBucketLock& other) =
        delete;

    ThreadSafeHashTableWithBucketLock(const int bucketSize = 19, const Hash& hasher = Hash());
    ~ThreadSafeHashTableWithBucketLock();

    void put(const K key, const V val);
    V get(const K& key);

    struct Bucket {
        std::shared_mutex _bucketLock;
        std::list<std::pair<K, V>> _bucketList;
        typename std::list<std::pair<K, V>>::iterator findEntry(const K& key) {
            return std::find_if(_bucketList.begin(),
                                _bucketList.end(),
                                [&](const std::pair<K, V>& item) { return item.first == key; });
        }
    };

private:
    std::mutex _printMutex;
    std::vector<std::unique_ptr<Bucket>> _buckets;
    Hash _hasher;
    const int _getBucketIndex(const K Key);
};

template <typename K, typename V, typename Hash>
ThreadSafeHashTableWithBucketLock<K, V, Hash>::ThreadSafeHashTableWithBucketLock(
    const int bucketSize, const Hash& hasher)
    : _hasher(hasher) {
    _buckets.reserve(bucketSize);
    for (int i = 0; i < bucketSize; i++) {
        auto ptr = std::make_unique<Bucket>();
        _buckets.emplace_back(std::move(ptr));
    }
}

template <typename K, typename V, typename Hash>
ThreadSafeHashTableWithBucketLock<K, V, Hash>::~ThreadSafeHashTableWithBucketLock() {}

template <typename K, typename V, typename Hash>
const int ThreadSafeHashTableWithBucketLock<K, V, Hash>::_getBucketIndex(const K key) {
    return _hasher(key) % _buckets.size();
}

template <typename K, typename V, typename Hash>
void ThreadSafeHashTableWithBucketLock<K, V, Hash>::put(const K key, const V val) {
    const int bucketIndex = _getBucketIndex(key);
    //   {
    //     std::lock_guard<std::mutex> lk(_printMutex);
    //     std::cout << "key: " << key << std::endl;
    //   }
    std::unique_lock<std::shared_mutex> lk(_buckets[bucketIndex]->_bucketLock);
    typename std::list<std::pair<K, V>>::iterator it = _buckets[bucketIndex]->findEntry(key);

    if (it == _buckets[bucketIndex]->_bucketList.end()) {
        _buckets[bucketIndex]->_bucketList.push_back(
            std::pair<K, V>(std::move(key), std::move(val)));
    } else {
        it->second = std::move(val);
    }
}

template <typename K, typename V, typename Hash>
V ThreadSafeHashTableWithBucketLock<K, V, Hash>::get(const K& key) {
    const int bucketIndex = _getBucketIndex(key);
    //   std::cout << "key: " << key << " with bucket index: " << bucketIndex
    //             << std::endl;
    std::shared_lock<std::shared_mutex> lk(_buckets[bucketIndex]->_bucketLock);
    typename std::list<std::pair<K, V>>::iterator it = _buckets[bucketIndex]->findEntry(key);
    V res;
    if (it == _buckets[bucketIndex]->_bucketList.end()) {
        return V();
    } else {
        return it->second;
    }
}

int main(int argc, char const* argv[]) {
    ThreadSafeHashTableWithBucketLock<int, int> table;
    int threadCnt = 10;
    int insertNum = 100;
    std::vector<std::thread> ts;
    ts.reserve(threadCnt);
    for (int i = 0; i < threadCnt; i++) {
        std::thread t(
            [&](int threadId, int insertNum) {
                for (int i = threadId * insertNum; i < threadId * insertNum + insertNum; i++) {
                    table.put(i, i);
                }
            },
            i,
            insertNum);
        ts.push_back(std::move(t));
    }

    for (int k = insertNum * threadCnt - 1; k >= 0; k--) {
        int v = table.get(k);
        std::cout << "[" << k << ", " << v << "]" << std::endl;
    }

    for (int i = 0; i < ts.size(); i++) {
        std::cout << "thread " << i << " join" << std::endl << std::flush;
        ts[i].join();
    }

    return 0;
}
