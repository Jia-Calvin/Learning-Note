#include "binary_tree.h"

void FindCore(BinaryNodeTree *root, int expectedSum, std::vector<int> &router,
              int currentSum) {
  currentSum += root->_val;
  router.push_back(root->_val);
  if (root->_leftNode == nullptr && root->_rightNode == nullptr) {
    if (expectedSum == currentSum) {
      printVector(router);
    } else {
      return;
    }
  }

  if (root->_leftNode != nullptr) {
    FindCore(root->_leftNode, expectedSum, router, currentSum);
  }
  if (root->_rightNode != nullptr) {
    FindCore(root->_rightNode, expectedSum, router, currentSum);
  }
  router.pop_back();
}

void FindPath(BinaryNodeTree *root, int expectedSum) {
  if (root == nullptr) {
    return;
  }

  std::vector<int> router;
  int currentSum = 0;

  FindCore(root, expectedSum, router, currentSum);
}

int main(int argc, char const *argv[]) {
  std::vector<int> preOrder = {1, 2, 4, 7, 3, 5, 6, 8};
  std::vector<int> middleOrder = {4, 7, 2, 1, 5, 3, 8, 6};

  BinaryNodeTree *root = rebuildBinaryTree(preOrder, middleOrder);
  printTree(root);

  FindPath(root, 14);

  std::vector<int> tt = {4, 7, 2, 1, 5, 3, 8, 6};
  std::vector<int> ttHeap;
  std::set<int> ttSet;
  for (auto &&ele : tt) {
    ttHeap.push_back(ele);
    ttSet.insert(ele);
    std::push_heap(ttHeap.begin(), ttHeap.end(),
                   [](int &left, int &right) { return left < right; });
  }
  printVector(ttHeap);
  printSet(ttSet);

  return 0;
}
