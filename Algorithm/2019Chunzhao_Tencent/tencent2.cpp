#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;

int main(int argc, char const* argv[]) {
    int length;
    string str;
    cin >> length >> str;

    stack<int> s;

    for (int i = 0; i < length; i++) {
        if (s.empty() || s.top() == str[i]) {
            s.push(str[i]);
        } else {
            s.pop();
        }
    }
    cout << s.size() << endl;
}