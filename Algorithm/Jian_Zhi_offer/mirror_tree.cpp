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
    void Mirror(TreeNode* pRoot) {
        if (pRoot == NULL) {
            return;
        }

        if (pRoot->right != NULL || pRoot->left != NULL) {
            TreeNode* tmp = pRoot->left;
            pRoot->left = pRoot->right;
            pRoot->right = tmp;
            Mirror(pRoot->left);
            Mirror(pRoot->right);
        }
    }
};

int main(int argc, char const* argv[]) {
    return 0;
}
