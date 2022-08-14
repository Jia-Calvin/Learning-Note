#include <algorithm>
#include <iostream>
#include <math.h>
#include <queue>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;
struct ListNode {
    int val;
    struct ListNode* next;
    ListNode(int x) : val(x), next(NULL) {}
};
class Solution {
public:
    ListNode* Merge(ListNode* pHead1, ListNode* pHead2) {
        if (pHead1 == NULL) {
            return pHead2;
        }
        if (pHead2 == NULL) {
            return pHead1;
        }

        ListNode* new_head = NULL;
        if (pHead1->val < pHead2->val) {
            new_head = pHead1;
            pHead1 = pHead1->next;
        } else {
            new_head = pHead2;
            pHead2 = pHead2->next;
        }

        ListNode* forward_node = new_head;
        while (pHead1 != NULL && pHead2 != NULL) {
            if (pHead1->val < pHead2->val) {
                forward_node->next = pHead1;
                pHead1 = pHead1->next;
            } else {
                forward_node->next = pHead2;
                pHead2 = pHead2->next;
            }

            forward_node = forward_node->next;
        }

        if (pHead1 != NULL) {
            forward_node->next = pHead1;
        } else {
            forward_node->next = pHead2;
        }

        return new_head;
    }
};
int main(int argc, char const* argv[]) {
    ListNode node(10);
    ListNode node1(9);
    ListNode node2(8);
    ListNode node3(7);

    node.next = &node1;
    node1.next = &node2;
    node2.next = &node3;

    Solution* s = new Solution();

    cout << s->Merge(&node)->val << endl;

    return 0;
}
