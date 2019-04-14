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
    struct TreeNode *left;
    struct TreeNode *right;
    TreeNode(int x) : val(x), left(NULL), right(NULL) {}
};
class Solution {
   public:
    vector<int> PrintFromTopToBottom(TreeNode *root) {
        vector<int> result;
        if (root == NULL) {
            return result;
        }
        queue<TreeNode *> tree_bfs;
        tree_bfs.push(root);
        while (tree_bfs.size() > 0) {
            TreeNode *root_cur = tree_bfs.front();
            result.push_back(root_cur->val);
            tree_bfs.pop();

            if (root_cur->left != NULL) {
                tree_bfs.push(root_cur->left);
            }
            if (root_cur->right != NULL) {
                tree_bfs.push(root_cur->right);
            }
        }
        return result;
    }
};