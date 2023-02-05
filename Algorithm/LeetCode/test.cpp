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
};

class Solution {
public:
};

int main(int argc, char const* argv[]) {
    Solution s;

    return 0;
}
