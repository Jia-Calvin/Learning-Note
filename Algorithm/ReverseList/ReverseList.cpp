#include <iostream>
using namespace std;

struct ListNode {
    int value;
    struct ListNode* next;
};

ListNode* reverseList(ListNode* header) {
    if (header->next == NULL || header == NULL) {
        return header;
    } else {
        ListNode* newHeader = reverseList(header->next);
        header->next->next = header;
        header->next = NULL;
        return newHeader;
    }
}

ListNode* reverseList(ListNode* header) {
    ListNode* pre = NULL;
    ListNode* curr = header;
    ListNode* next = header->next;

    while (next != NULL) {
        curr->next = pre;
        pre = curr;
        curr = next;
        next = next->next;
    }
    curr->next = pre;
    header = curr;
    return header;
}

void printList(ListNode* header) {
    ListNode* ptr = header;
    while (ptr != NULL) {
        printf("%d ", ptr->value);
        ptr = ptr->next;
    }
    printf("\n");
}

int main(int argc, char const* argv[]) {
    ListNode* header;
    header = (ListNode*)malloc(sizeof(header));
    header->value = 10;
    header->next = NULL;

    for (int i = 0; i < 10; i++) {
        ListNode* node;
        node = (ListNode*)malloc(sizeof(node));

        node->value = i;

        node->next = header->next;

        header->next = node;
    }

    printList(header);

    header = reverseList(header);

    printList(header);

    return 0;
}
