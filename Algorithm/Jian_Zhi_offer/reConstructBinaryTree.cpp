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
    TreeNode* constructBinaryTree(vector<int> pre, vector<int> vin) {
        cout << pre.size() << ", " << vin.size() << endl;
        int root_value = pre[0];
        TreeNode* root = new TreeNode(root_value);
        if (pre.size() == 1) {
            if (vin.size() == 1 && pre[0] == vin[0]) {
                return root;
            } else {
                printf("Invaild input.");
                exit(1);
            }
        }

        int cut = NULL;
        bool flag = false;
        for (int i = 0; i < vin.size(); i++) {
            if (vin[i] == root_value) {
                cut = i;
                flag = true;
            }
            if (i == vin.size() - 1 && !flag) {
                printf("Invaild input.");
                exit(1);
            }
        }

        if (cut > 0) {
            root->left = constructBinaryTree(
                vector<int>(pre.begin() + 1, pre.begin() + cut + 1),
                vector<int>(vin.begin(), vin.begin() + cut));
        }
        if (vin.size() - 1 - cut > 0) {
            root->right = constructBinaryTree(
                vector<int>(pre.begin() + cut + 1, pre.end()),
                vector<int>(vin.begin() + cut + 1, vin.end()));
        }

        return root;
    }

    TreeNode* reConstructBinaryTree(vector<int> pre, vector<int> vin) {
        if (pre.size() != vin.size() || pre.size() == 0 || vin.size() == 0) {
            return NULL;
        }

        return constructBinaryTree(pre, vin);
    }
};

void printTree(TreeNode* node) {
    if (node != NULL) {
        cout << node->val << ", ";
        printTree(node->left);
        printTree(node->right);
    }
}

int main(int argc, char const* argv[]) {
    // vector<int> pre{5, 4, 1, 2, 9, 3, 7, 10, 8};
    // vector<int> vin{1, 4, 9, 2, 5, 7, 10, 3, 8};
    vector<int> pre{5, 4, 1, 3};
    vector<int> vin{3, 1, 4, 5};

    Solution* s = new Solution();

    TreeNode* result = s->reConstructBinaryTree(pre, vin);
    printTree(result);
    cout << endl;

    return 0;
}
