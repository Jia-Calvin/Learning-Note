#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

template <class T>
class ConsistHash {
public:
    ConsistHash(int repl) : _repl(repl) {}

    void put(T node);
    T get(T key);

private:
    std::mutex _mutex;
    std::unordered_map<std::size_t, T> _consistMap;
    std::vector<std::size_t> _sortedNode;
    int _repl;
};

template <class T>
void ConsistHash<T>::put(T node) {
    std::lock_guard<std::mutex> lk(_mutex);
    for (int i = 0; i < _repl; i++) {
        std::size_t hashVal = std::hash<std::string>{}(node + std::to_string(i));
        _consistMap[hashVal] = node;
        _sortedNode.push_back(hashVal);
    }
    std::sort(_sortedNode.begin(), _sortedNode.end());
}

template <class T>
T ConsistHash<T>::get(T key) {
    std::lock_guard<std::mutex> lk(_mutex);
    std::size_t hashVal = std::hash<std::string>{}(key);
    std::vector<std::size_t>::const_iterator it =
        std::lower_bound(_sortedNode.begin(), _sortedNode.end(), hashVal);
    if (it == _sortedNode.end()) {
        return _consistMap[_sortedNode.front()];
    }
    return _consistMap[*it];
}

int main() {
    ConsistHash<std::string> myHash(100);
    myHash.put("123");
    myHash.put("456");
    std::cout << myHash.get("333") << std::endl;
    std::cout << myHash.get("444") << std::endl;
    myHash.put("789");
    myHash.put("101112");
    std::cout << myHash.get("333") << std::endl;
    myHash.put("131415");
    std::cout << myHash.get("333") << std::endl;
    std::cout << myHash.get("444") << std::endl;
    std::cout << myHash.get("444") << std::endl;

    return 0;
}
