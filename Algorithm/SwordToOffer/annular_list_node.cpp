#include <iostream>
#include <list>
#include <memory>
#include <stack>
#include <vector>

struct ListNode {
  int _val;
  ListNode *_pNext;
  ListNode(int val) : _val(val){};
};

// 链表是否有环
ListNode *isAnnularList(ListNode *pHead) {
  if (pHead == nullptr) {
    return nullptr;
  }
  ListNode *pSlow = pHead->_pNext;
  if (pSlow == nullptr) {
    return nullptr;
  }
  ListNode *pFast = pSlow->_pNext;
  while (pSlow != nullptr && pFast != nullptr) {
    if (pSlow == pFast) {
      return pSlow;
    }

    pSlow = pSlow->_pNext;
  
    pFast = pFast->_pNext;
    if (pFast != nullptr) {
      pFast = pFast->_pNext;
    }
  }
  return nullptr;
}

ListNode *reverseCore(ListNode *node, ListNode *pre) {
  if (node == nullptr) {
    return pre;
  }
  ListNode *newHead = reverseCore(node->_pNext, node);
  node->_pNext = pre;
  return newHead;
}

ListNode *reverserList(ListNode *pHead) {
  if (pHead == nullptr) {
    return nullptr;
  }
  return reverseCore(pHead, nullptr);
}

// 环的入口
ListNode *getAnnularInputNode(ListNode *pHead, ListNode *pAnnularNode) {
  if (pHead == nullptr) {
    return nullptr;
  }
  int annularLength = 0;
  ListNode *pTmp = pAnnularNode;
  while (pTmp != pAnnularNode) {
    pTmp = pTmp->_pNext;
    annularLength++;
  }

  ListNode *pAHead = pHead;
  ListNode *pBehind = pHead;
  while (annularLength != 0) {
    pAHead = pAHead->_pNext;
    annularLength--;
  }

  while (pBehind != nullptr && pAHead != nullptr) {
    if (pAHead == pBehind) {
      return pAHead;
    }
    pAHead = pAHead->_pNext;
    pBehind = pBehind->_pNext;
  }

  return nullptr;
}

int main(int argc, char const *argv[]) {
  ListNode *node1 = new ListNode(1);
  ListNode *node2 = new ListNode(2);
  ListNode *node3 = new ListNode(3);
  ListNode *node4 = new ListNode(4);
  node1->_pNext = node2;
  node2->_pNext = node3;
  node3->_pNext = node4;

  ListNode *annularNode = isAnnularList(node1);
  if (annularNode != nullptr) {
    ListNode *inputNode = getAnnularInputNode(node1, annularNode);
  } else {
    std::cout << "node annular" << std::endl;
  }

  ListNode *newHead = reverserList(node1);
  while (newHead != nullptr) {
    std::cout << newHead->_val << std::endl;
    newHead = newHead->_pNext;
  }

  return 0;
}
