#include <algorithm>
#include <iostream>
#include <math.h>
#include <queue>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;

struct TreeNode {
    int val;
    struct TreeNode* left;
    struct TreeNode* right;
    TreeNode(int x) : val(x), left(NULL), right(NULL) {}
};

class Solution {
public:
    bool subTree(TreeNode* pRoot1, TreeNode* pRoot2) {
        if (pRoot2 == NULL) {
            return true;
        } else if (pRoot1 == NULL) {
            return false;
        }

        if (pRoot1->val == pRoot2->val) {
            return subTree(pRoot1->left, pRoot2->left) && subTree(pRoot1->right, pRoot2->right);
        }
        return false;
    }

    bool HasSubtree(TreeNode* pRoot1, TreeNode* pRoot2) {
        if (pRoot1 == NULL || pRoot2 == NULL) {
            return false;
        }

        int issubtree = false;
        if (pRoot1->val == pRoot2->val) {
            issubtree = subTree(pRoot1, pRoot2);
        }

        return HasSubtree(pRoot1->left, pRoot2) || HasSubtree(pRoot1->right, pRoot2) || issubtree;
    }
};

int main(int argc, char const* argv[]) {
    TreeNode node1(8);
    TreeNode node2(8);
    TreeNode node3(7);
    TreeNode node4(9);
    TreeNode node5(2);
    TreeNode node6(4);
    TreeNode node7(7);

    node1.left = &node2;
    node1.right = &node3;
    node2.left = &node4;
    node2.right = &node5;
    node5.left = &node6;
    node5.right = &node7;

    TreeNode node8(8);
    TreeNode node9(9);
    TreeNode node10(2);

    node8.left = &node9;
    node8.right = &node10;

    Solution* s = new Solution();
    cout << s->HasSubtree(&node1, &node8) << endl;

    return 0;
}
