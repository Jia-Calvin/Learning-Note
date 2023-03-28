#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>
using namespace std;

// 非线程安全的，需要加锁，或设计无锁结构
class LRUCache {
public:
    LRUCache(int capacity) {
        _cap = capacity;
    }
    ~LRUCache() = delete;

    int get(int key) {
        if (_keyToLst.find(key) != _keyToLst.end()) {
            put(key, _keyToLst[key]->second);
            return _keyToLst[key]->second;
        }
        return -1;
    }

    void put(int key, int value) {
        if (_keyToLst.find(key) != _keyToLst.end()) {
            _lst.erase(_keyToLst[key]);
        } else if (_lst.size() >= _cap) {
            _keyToLst.erase(_lst.back().first);
            _lst.pop_back();
        }
        _lst.push_front(pair<int, int>(key, value));
        _keyToLst[key] = _lst.begin();
    }

private:
    int _cap;
    list<pair<int, int>> _lst;
    unordered_map<int, list<pair<int, int>>::iterator> _keyToLst;
};

/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */

int main(int argc, char const* argv[]) {
    LRUCache* lRUCache = new LRUCache(2);
    lRUCache->put(1, 1);               // cache is {1=1}
    lRUCache->put(2, 2);               // cache is {1=1, 2=2}
    cout << lRUCache->get(1) << endl;  // return 1
    lRUCache->put(3, 3);               // LRU key was 2, evicts key 2, cache is {1=1, 3=3}
    cout << lRUCache->get(2) << endl;  // returns -1 (not found)
    lRUCache->put(4, 4);               // LRU key was 1, evicts key 1, cache is {4=4, 3=3}
    cout << lRUCache->get(1) << endl;  // return -1 (not found)
    cout << lRUCache->get(3) << endl;  // return 3
    cout << lRUCache->get(4) << endl;  // return 4
    return 0;
}
