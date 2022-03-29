#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>
using namespace std;

class LFUCache {
   public:
    int cap;
    int minFreq;
    unordered_map<int, int> keyToValue;
    unordered_map<int, int> keyToFreq;
    unordered_map<int, list<int> > freqToKey;

    void increaseFreq(int key) {
        int curFreq = keyToFreq[key];
        freqToKey[curFreq].remove(key);
        keyToFreq[key]++;
        freqToKey[curFreq + 1].push_back(key);
        if (freqToKey[curFreq].empty()) {
            freqToKey.erase(curFreq);
            if (minFreq == curFreq) minFreq++;
        }
    }

    void removeMinFreqKey() {
        int minKey = freqToKey[minFreq].front();
        freqToKey[minFreq].pop_front();
        if (freqToKey[minFreq].empty()) {
            freqToKey.erase(minFreq);
        }
        keyToValue.erase(minKey);
        keyToFreq.erase(minKey);
    }

    LFUCache(int capacity) {
        cap = capacity;
        minFreq = 0;
    }

    int get(int key) {
        if (keyToValue.find(key) != keyToValue.end()) {
            increaseFreq(key);
            return keyToValue[key];
        }
        return -1;
    }

    void put(int key, int value) {
        if (cap <= 0) return;
        if (keyToValue.find(key) != keyToValue.end()) {
            keyToValue[key] = value;
            increaseFreq(key);
        } else {
            if (keyToFreq.size() >= cap) {
                removeMinFreqKey();
            }
            keyToValue[key] = value;
            keyToFreq[key] = 1;
            minFreq = 1;
            freqToKey[1].push_back(key);
        }
    }
};

int main(int argc, char const *argv[]) {
    LFUCache *cache = new LFUCache(2);
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