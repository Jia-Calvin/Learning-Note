#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class ConsistHash {
public:
  ConsistHash(int repl) : _repl(repl) {}

  void put(std::string node);
  std::string get(std::string key);

private:
  std::mutex _consistMapPLock;
  std::unordered_map<std::size_t, std::string> _consistMap;
  std::vector<std::size_t> _sortedNode;
  int _repl;
};

void ConsistHash::put(std::string node) {
  std::lock_guard<std::mutex> lk(_consistMapPLock);
  for (int i = 0; i < _repl; i++) {
    std::size_t hashVal = std::hash<std::string>{}(node + std::to_string(i));
    _consistMap[hashVal] = node;
    _sortedNode.push_back(hashVal);
  }
  std::sort(_sortedNode.begin(), _sortedNode.end());
}

std::string ConsistHash::get(std::string key) {
  std::lock_guard<std::mutex> lk(_consistMapPLock);
  std::size_t hashVal = std::hash<std::string>{}(key);
  std::vector<std::size_t>::const_iterator it =
      std::lower_bound(_sortedNode.begin(), _sortedNode.end(), hashVal);
  return _consistMap[*it];
}

int main() {
  ConsistHash myHash(10);
  myHash.put("123");
  myHash.put("456");
  std::cout << myHash.get("333") << std::endl;
  myHash.put("789");
  myHash.put("101112");
  std::cout << myHash.get("333") << std::endl;
  myHash.put("131415");
  std::cout << myHash.get("333") << std::endl;

  return 0;
}
