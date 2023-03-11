#include <atomic>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode* next) : val(x), next(next) {}
};

struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode* left, TreeNode* right) : val(x), left(left), right(right) {}
};

class Node {
public:
    int val;
    Node* left;
    Node* right;
    Node* next;

    Node() : val(0), left(NULL), right(NULL), next(NULL) {}

    Node(int _val) : val(_val), left(NULL), right(NULL), next(NULL) {}

    Node(int _val, Node* _left, Node* _right, Node* _next)
        : val(_val), left(_left), right(_right), next(_next) {}
};


class MyPrinter {
protected:
    MyPrinter(/* args */) = default;
    ~MyPrinter() = default;


    // 中序遍历，递归方法
    static void printTreeCore(TreeNode* root) {
        if (root == nullptr) {
            return;
        }

        if (root->left != nullptr) {
            printTreeCore(root->left);
        }

        std::cout << root->val << ", ";

        if (root->right != nullptr) {
            printTreeCore(root->right);
        }
    }

    template <class T>
    static void printVectorCore(std::vector<T>& vec) {
        for (auto&& ele : vec) {
            std::cout << ele << ",";
        }
        std::cout << std::endl;
    }

public:
    // 中序遍历，递归方法
    static void printTree(TreeNode* root) {
        printTreeCore(root);
        std::cout << std::endl;
    }

    static void printList(ListNode* node) {
        while (node != NULL) {
            std::cout << node->val << ",";
            node = node->next;
        }
        std::cout << std::endl;
    }

    template <class T>
    static void printVector(std::vector<T>& vec) {
        printVectorCore(vec);
    }

    template <class T>
    static void print2DVector(std::vector<std::vector<T>>& vec) {
        for (auto&& firstVec : vec) {
            printVectorCore(firstVec);
        }
        std::cout << std::endl;
    }

    template <class K, class V, class C>
    static void printMap(std::map<K, V, C>& maps) {
        for (auto&& kv : maps) {
            std::cout << kv.first << ", [";
            for (auto&& ele : kv.second) {
                std::cout << ele << ", ";
            }
            std::cout << "]" << std::endl;
        }
        std::cout << std::endl;
    }
};

class Solution {
public:
    int leastInterval(vector<char>& tasks, int n) {
        if (n == 0) {
            return tasks.size();
        }
        std::unordered_map<char, int> keyToFre;
        std::map<int, std::list<char>, std::greater<int>> freToKeyList;
        for (int i = 0; i < tasks.size(); i++) {
            keyToFre[tasks[i]]++;
        }
        for (auto iter = keyToFre.begin(); iter != keyToFre.end(); iter++) {
            freToKeyList[iter->second].push_back(iter->first);
        }
        int left = 0, right = -1;
        vector<char> rebuildTasks;
        std::unordered_map<char, bool> usedTask;
        while (!freToKeyList.empty()) {
            MyPrinter::printMap(freToKeyList);
            bool getEle = false;
            for (auto iterMap = freToKeyList.begin(); iterMap != freToKeyList.end(); iterMap++) {
                for (auto iterList = iterMap->second.begin(); iterList != iterMap->second.end();
                     iterList++) {
                    char element = *iterList;
                    if (!usedTask[element]) {
                        rebuildTasks.push_back(element);
                        MyPrinter::printVector(rebuildTasks);
                        usedTask[element] = true;
                        if (iterMap->first != 1) {
                            freToKeyList[iterMap->first - 1].push_back(element);
                        }

                        iterMap->second.erase(iterList);
                        if (iterMap->second.empty()) {
                            freToKeyList.erase(iterMap);
                        }

                        right++;
                        if (right - left + 1 > n) {
                            usedTask[rebuildTasks[left]] = false;
                            left++;
                        }
                        getEle = true;
                        break;
                    }
                }
                if (getEle) {
                    break;
                }
            }
            if (!getEle) {
                rebuildTasks.push_back('0');
                right++;
                if (right - left + 1 > n) {
                    usedTask[rebuildTasks[left]] = false;
                    left++;
                }
            }
        }
        return rebuildTasks.size();
    }
};

int main(int argc, char const* argv[]) {
    Solution s;
    vector<char> nums = {'A', 'A', 'A', 'B', 'B', 'B'};
    cout << s.leastInterval(nums, 50) << endl;

    // nums = {'A', 'A', 'A', 'A', 'A', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};
    // cout << s.leastInterval(nums, 2) << endl;

    return 0;
}
