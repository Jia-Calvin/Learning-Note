#include <math.h>
#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;

class Solution {
   public:
    void push(int node) { stack1.push(node); }

    int pop() {
        if (stack2.empty()) {
            while (!stack1.empty()) {
                int tmp = stack1.top();
                stack2.push(tmp);
                stack1.pop();
            }
        }
        int top_elem;
        if (!stack2.empty()) {
            top_elem = stack2.top();
            stack2.pop();
        }
        return top_elem;
    }

   private:
    stack<int> stack1;
    stack<int> stack2;
};

int main(int argc, char const *argv[]) {
    Solution *s = new Solution();

    s->push(1);
    s->push(3);
    s->push(3);
    s->push(5);
    s->push(7);

    cout << s->pop() << endl;
    cout << s->pop() << endl;
    cout << s->pop() << endl;
    cout << s->pop() << endl;
    cout << s->pop() << endl;
    cout << s->pop() << endl;
    cout << s->pop() << endl;

    return 0;
}
