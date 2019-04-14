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
    void Path(TreeNode* root, int expectNumber, vector<int>& one_route,
              vector<vector<int> >& comb) {
        int root_value = root->val;
        if (expectNumber == root_value && root->left == NULL &&
            root->right == NULL) {
            one_route.push_back(root_value);
            comb.push_back(one_route);
            return;
        }

        if (root != NULL) {
            one_route.push_back(root_value);
            if (root->left != NULL) {
                Path(root->left, expectNumber - root_value, one_route, comb);
                one_route.pop_back();
            }
            if (root->right != NULL) {
                Path(root->right, expectNumber - root_value, one_route, comb);
                one_route.pop_back();
            }
        }
    }

    vector<vector<int> > FindPath(TreeNode* root, int expectNumber) {
        vector<vector<int> > comb;
        if (root == NULL) {
            return comb;
        }
        vector<int> one_route;
        Path(root, expectNumber, one_route, comb);

        return comb;
    }
};

int main(int argc, char const* argv[]) { return 0; }
