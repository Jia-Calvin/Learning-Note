#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>
using namespace std;

// 非线程安全的，需要加锁，或设计无锁结构
class LFUCache {
public:
    LFUCache(int capacity) {
        _cap = capacity;
        _minFreq = 0;
    }
    ~LFUCache() = default;

    void increaseFreq(int key) {
        int curFreq = _keyToFreq[key];
        _freqToKey[curFreq].remove(key);
        _keyToFreq[key]++;
        _freqToKey[curFreq + 1].push_back(key);
        if (_freqToKey[curFreq].empty()) {
            _freqToKey.erase(curFreq);
            if (_minFreq == curFreq)
                _minFreq++;
        }
    }

    void removeMinFreqKey() {
        int minKey = _freqToKey[_minFreq].front();
        _freqToKey[_minFreq].pop_front();
        if (_freqToKey[_minFreq].empty()) {
            _freqToKey.erase(_minFreq);
        }
        _keyToValue.erase(minKey);
        _keyToFreq.erase(minKey);
    }


    int get(int key) {
        if (_keyToValue.find(key) != _keyToValue.end()) {
            increaseFreq(key);
            return _keyToValue[key];
        }
        return -1;
    }

    void put(int key, int value) {
        if (_cap <= 0)
            return;
        if (_keyToValue.find(key) != _keyToValue.end()) {
            _keyToValue[key] = value;
            increaseFreq(key);
        } else {
            if (_keyToFreq.size() >= _cap) {
                removeMinFreqKey();
            }
            _keyToValue[key] = value;
            _keyToFreq[key] = 1;
            _minFreq = 1;
            _freqToKey[1].push_back(key);
        }
    }

private:
    int _cap;
    int _minFreq;
    unordered_map<int, int> _keyToValue;
    unordered_map<int, int> _keyToFreq;
    unordered_map<int, list<int>> _freqToKey;
};

int main(int argc, char const* argv[]) {
    LFUCache* cache = new LFUCache(2);
    cache->put(1, 1);
    cache->put(2, 2);
    cout << cache->get(1) << endl;  // returns 1
    cache->put(3, 3);               // evicts key 2
    cout << cache->get(2) << endl;  // returns -1 (not found)
    cout << cache->get(3) << endl;  // returns 3.
    cache->put(4, 4);               // evicts key 1.
    cout << cache->get(1) << endl;  // returns -1 (not found)
    cout << cache->get(3) << endl;  // returns 3
    cout << cache->get(4) << endl;  // returns 4
    return 0;
}