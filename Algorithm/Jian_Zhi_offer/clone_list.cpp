#include <math.h>
#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;

struct RandomListNode {
    int label;
    struct RandomListNode *next, *random;
    RandomListNode(int x) : label(x), next(NULL), random(NULL) {}
};

class Solution {
   public:
    void clone_node(RandomListNode* pHead) {
        RandomListNode* cp_head = pHead;
        while (cp_head != NULL) {
            RandomListNode* next_node = cp_head->next;
            RandomListNode* copy_node = new RandomListNode(cp_head->label);
            cp_head->next = copy_node;
            copy_node->next = next_node;
            cp_head = next_node;
        }
    }

    void link_copy_node(RandomListNode* pHead) {
        RandomListNode* cp_head = pHead;
        while (cp_head != NULL) {
            if (cp_head->random != NULL) {
                cp_head->next->random = cp_head->random->next;
            }
            cp_head = cp_head->next->next;
        }
    }

    RandomListNode* cut_list(RandomListNode* pHead) {
        RandomListNode* cp_head = pHead;
        RandomListNode* re_head = pHead->next;
        RandomListNode* frwa_head = pHead->next;

        while (frwa_head->next != NULL) {
            cp_head->next = frwa_head->next;
            frwa_head->next = frwa_head->next->next;
            cp_head = cp_head->next;
            frwa_head = frwa_head->next;
        }

        cp_head->next = NULL;

        return re_head;
    }

    RandomListNode* Clone(RandomListNode* pHead) {
        if (pHead == NULL) {
            return pHead;
        }
        clone_node(pHead);
        link_copy_node(pHead);

        return cut_list(pHead);
    }
};
int main(int argc, char const* argv[]) { return 0; }
