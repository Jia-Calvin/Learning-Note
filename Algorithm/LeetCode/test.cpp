#include <atomic>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>

struct ListNode {
  int val;
  ListNode *next;
  ListNode() : val(0), next(nullptr) {}
  ListNode(int x) : val(x), next(nullptr) {}
  ListNode(int x, ListNode *next) : val(x), next(next) {}
};

struct TreeNode {
  int val;
  TreeNode *left;
  TreeNode *right;
  TreeNode() : val(0), left(nullptr), right(nullptr) {}
  TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
  TreeNode(int x, TreeNode *left, TreeNode *right)
      : val(x), left(left), right(right) {}
};

class Node {
public:
  int val;
  Node *left;
  Node *right;
  Node *next;

  Node() : val(0), left(NULL), right(NULL), next(NULL) {}

  Node(int _val) : val(_val), left(NULL), right(NULL), next(NULL) {}

  Node(int _val, Node *_left, Node *_right, Node *_next)
      : val(_val), left(_left), right(_right), next(_next) {}
};

// 中序遍历，递归方法
void printTreeCore(TreeNode *root) {
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

// 中序遍历，递归方法
void printTree(TreeNode *root) {
  printTreeCore(root);
  std::cout << std::endl;
}

template <class T> void printVectorCore(std::vector<T> &vec) {
  for (auto ele : vec) {
    std::cout << ele << ",";
  }
}

template <class T> void printVector(std::vector<T> &vec) {
  printVectorCore(vec);
  std::cout << std::endl;
}

void printList(ListNode *node) {
  while (node != NULL) {
    std::cout << node->val << ",";
    node = node->next;
  }
  std::cout << std::endl;
}

using namespace std;
class Solution {
public:
  int titleToNumber(string columnTitle) {
    int ans = 0;
    int p = 0;
    for (int i = columnTitle.size() - 1; i >= 0; i--) {
      ans += (columnTitle[i] - 'A' + 1) * std::pow(26, p++);
    }
    return ans;
  }
};

// int main(int argc, char const *argv[]) {
//   Solution s;
//   vector<int> nums{2, 3, -2, 4};
//   std::cout << s.maxProduct(nums) << std::endl;
//   vector<int> nums1{-2, 0, -1};
//   std::cout << s.maxProduct(nums1) << std::endl;
//   vector<int> nums2{-1, -2, -9, -6};
//   std::cout << s.maxProduct(nums2) << std::endl;
//   return 0;
// }

void fun(int &a) {
  std::cout << "in fun(int &)" << std::endl;
}

void fun(int &&a) {
  std::cout << "in fun(int &&)" << std::endl;
}

int main() {
  int a = 1;
  int &&b = 1;
  
  fun(std::move(b));
  
  return 0;
}