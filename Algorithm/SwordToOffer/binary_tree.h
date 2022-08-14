#pragma once

#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <stack>
#include <vector>

struct BinaryNodeTree {
    int _val;
    BinaryNodeTree* _leftNode;
    BinaryNodeTree* _rightNode;

    BinaryNodeTree(int val) : _val(val){};
};

void printSetCore(std::set<int>& s) {
    for (auto& ele : s) {
        std::cout << ele << ",";
    }
}

void printSet(std::set<int>& s) {
    printSetCore(s);
    std::cout << std::endl;
}

void printVectorCore(std::vector<int>& vec) {
    for (auto& ele : vec) {
        std::cout << ele << ",";
    }
}

void printVector(std::vector<int>& vec) {
    printVectorCore(vec);
    std::cout << std::endl;
}

// 中序遍历，递归方法
void printTreeCore(BinaryNodeTree* root) {
    if (root == nullptr) {
        return;
    }

    if (root->_leftNode != nullptr) {
        printTreeCore(root->_leftNode);
    }

    std::cout << root->_val << ", ";

    if (root->_rightNode != nullptr) {
        printTreeCore(root->_rightNode);
    }
}

// 中序遍历，递归方法
void printTree(BinaryNodeTree* root) {
    printTreeCore(root);
    std::cout << std::endl;
}

// 中序遍历，循环方法
void printTreeLoop(BinaryNodeTree* root) {
    if (root == nullptr) {
        return;
    }

    std::stack<BinaryNodeTree*> myStack;
    BinaryNodeTree* pCurrent = root;

    while (pCurrent || myStack.size() > 0) {
        // 左儿子如果存在一直入栈，为了回溯的时候能够找到对应的节点
        while (pCurrent) {
            myStack.push(pCurrent);
            pCurrent = pCurrent->_leftNode;
        }
        // 回溯，拿栈顶元素，最近入栈元素
        pCurrent = myStack.top();
        std::cout << pCurrent->_val << ", ";
        myStack.pop();
        pCurrent = pCurrent->_rightNode;
    }
}

// 根据前序遍历和中序遍历，重建二叉树
BinaryNodeTree* rebuildBinaryTree(std::vector<int> preOrder, std::vector<int> middleOrder) {
    if (preOrder.size() == 0 || middleOrder.size() == 0) {
        return nullptr;
    }

    if (preOrder.size() != middleOrder.size()) {
        throw "the input preOrder and middleOrder is illegal";
    }

    int rootVal = preOrder[0];
    BinaryNodeTree* root = new BinaryNodeTree(rootVal);
    int leftTreeNum = 0;
    for (auto& midEle : middleOrder) {
        if (midEle == rootVal) {
            break;
        }
        leftTreeNum++;
    }

    root->_leftNode = rebuildBinaryTree(
        std::vector<int>(preOrder.begin() + 1, preOrder.begin() + leftTreeNum + 1),
        std::vector<int>(middleOrder.begin(), middleOrder.begin() + leftTreeNum));
    root->_rightNode = rebuildBinaryTree(
        std::vector<int>(preOrder.begin() + leftTreeNum + 1, preOrder.end()),
        std::vector<int>(middleOrder.begin() + leftTreeNum + 1, middleOrder.end()));

    return root;
}