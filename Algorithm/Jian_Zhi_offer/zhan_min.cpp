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
    void push(int value) {
        stck.push(value);
        if (min_value > value) {
            min_value = value;
        }
        min_stck.push(min_value);
    }
    void pop() {
        stck.pop();
        min_stck.pop();
    }
    int top() { return stck.top(); }
    int min() { return min_stck.top(); }

   private:
    int min_value = 99999999999;
    stack<int> stck;
    stack<int> min_stck;
};

int main(int argc, char const* argv[]) { return 0; }
