#include <math.h>
#include <algorithm>
#include <iostream>
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
    vector<int> printListFromTailToHead(ListNode* head) {
        vector<int> list;

        stack<int> s;

        while (head != NULL) {
            s.push(head->val);
            head = head->next;
        }

        while (!s.empty()) {
            list.push_back(s.top());
            s.pop();
        }

        return list;
    }
};

class Solution {
   public:
    void printList(ListNode* head, vector<int>& list) {
        if (head == NULL) {
            return;
        }

        printList(head->next, list);
        list.push_back(head->val);
    }

    vector<int> printListFromTailToHead(ListNode* head) {
        vector<int> list;

        printList(head, list);

        return list;
    }
};

int main(int argc, char const* argv[]) { return 0; }
