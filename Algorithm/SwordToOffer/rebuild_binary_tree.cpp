#include "binary_tree.h"

int main(int argc, char const* argv[]) {
    std::vector<int> preOrder = {1, 2, 4, 7, 3, 5, 6, 8};
    std::vector<int> middleOrder = {4, 7, 2, 1, 5, 3, 8, 6};

    BinaryNodeTree* root = rebuildBinaryTree(preOrder, middleOrder);

    printTree(root);
    std::cout << std::endl;

    printTreeLoop(root);
    std::cout << std::endl;

    preOrder = {1, 3, 2};
    middleOrder = {3, 1, 2};

    root = rebuildBinaryTree(preOrder, middleOrder);

    printTree(root);
    std::cout << std::endl;

    return 0;
}