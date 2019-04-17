#include <math.h>
#include <algorithm>
#include <iostream>
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
    void ConverNode(TreeNode* pRoot, TreeNode** pLastList) {
        if (pRoot == NULL) {
            return;
        }
        TreeNode* pCurrNode = pRoot;
        if (pCurrNode->left != NULL) {
            ConverNode(pCurrNode->left, pLastList);
        }
        pCurrNode->left = *pLastList;
        if (*pLastList != NULL) {
            (*pLastList)->right = pCurrNode;
        }

        *pLastList = pCurrNode;
        if (pCurrNode->right != NULL) {
            ConverNode(pCurrNode->right, pLastList);
        }
    }

    TreeNode* Convert(TreeNode* pRoot) {
        if (pRoot == NULL) {
            return pRoot;
        }
        TreeNode* pLastList = NULL;
        ConverNode(pRoot, &pLastList);

        TreeNode* pHeadList = pLastList;
        while (pHeadList->left != NULL) {
            pHeadList = pHeadList->left;
        }
        return pHeadList;
    }
};

int main(int argc, char const* argv[]) {
    TreeNode* node1 = new TreeNode(10);
    TreeNode* node2 = new TreeNode(9);
    TreeNode* node3 = new TreeNode(8);
    TreeNode* node4 = new TreeNode(7);

    node1->left = node3;
    node3->left = node4;
    node3->right = node2;

    Solution* s = new Solution();
    s->Convert(node1);

    return 0;
}
