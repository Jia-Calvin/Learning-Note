#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>
using namespace std;

class LRUCache {
public:
    int cap;
    list<pair<int, int>> lst;
    unordered_map<int, list<pair<int, int>>::iterator> mp;

    LRUCache(int capacity) {
        cap = capacity;
    }

    int get(int key) {
        if (mp.find(key) != mp.end()) {
            put(key, mp[key]->second);
            return mp[key]->second;
        }
        return -1;
    }

    void put(int key, int value) {
        if (mp.find(key) != mp.end()) {
            lst.erase(mp[key]);
        } else if (lst.size() >= cap) {
            mp.erase(lst.back().first);
            lst.pop_back();
        }
        lst.push_front(pair<int, int>(key, value));
        mp[key] = lst.begin();
    }
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
