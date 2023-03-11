#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
int main() {
    int n;
    cin >> n;
    vector<string> sArray;
    for (int i = 0; i < n; i++) {
        string temp;
        cin >> temp;
        sArray.push_back(temp);
    }
    sort(sArray.begin(), sArray.end(), [](string s1, string s2) { return (s1 + s2) > (s2 + s1); });
    for (auto s : sArray) {
        cout << s;
    }
    return 0;
}