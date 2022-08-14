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
    ListNode* ReverseList(ListNode* pHead) {
        if (pHead == NULL) {
            return NULL;
        }

        ListNode* pre_head = NULL;
        ListNode* current_head = pHead;

        while (current_head != NULL) {
            ListNode* next_head = current_head->next;

            current_head->next = pre_head;
            pre_head = current_head;
            current_head = next_head;
        }

        return pre_head;
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

    cout << s->ReverseList(&node)->val << endl;

    return 0;
}
