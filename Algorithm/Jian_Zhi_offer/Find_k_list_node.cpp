#include <algorithm>
#include <iostream>
#include <math.h>
#include <sstream>
#include <vector>
using namespace std;

struct ListNode {
    int val;
    struct ListNode* next;
    ListNode(int x) : val(x), next(NULL) {}
};
class Solution {
public:
    ListNode* FindKthToTail(ListNode* pListHead, unsigned int k) {
        ListNode* new_head = pListHead;
        for (int i = 0; i < k - 1; i++) {
            if (new_head != NULL) {
                new_head = new_head->next;
            } else {
                return NULL;
            }
        }

        if (new_head == NULL) {
            return NULL;
        }

        while (new_head->next != NULL) {
            new_head = new_head->next;
            pListHead = pListHead->next;
        }

        return pListHead;
    }
};

int main(int argc, char const* argv[]) {
    ListNode node(10);
    ListNode node1(9);
    ListNode node2(8);
    node.next = &node1;
    node1.next = &node2;

    Solution* s = new Solution();
    cout << s->FindKthToTail(NULL, 4)->val << endl;

    return 0;
}
