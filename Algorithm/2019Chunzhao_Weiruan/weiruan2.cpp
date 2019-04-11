#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>
using namespace std;

// intput1: n, input2: pos, input3: max_step

int maxCir(int input1, int input2, vector<vector<int>> input3) {
    vector<int> queue;
    for (int i = 0; i < input1; i++) {
        queue.push_back(i + 1);
    }

    int sum = 0;

    for (int i = 0; i < input2; i++) {
        if (input3[i][0] == 1) {
            queue.erase(queue.begin());
        }

        if (input3[i][0] == 2) {
            int pos = 0;
            for (int j = 0; j < queue.size(); j++) {
                if (queue[j] == input3[i][1]) {
                    pos = j;
                    break;
                }
            }
            queue.erase(queue.begin() + pos);
        }

        if (input3[i][0] == 3) {
            int pos = 0;
            for (int j = 0; j < queue.size(); j++) {
                if (queue[j] == input3[i][1]) {
                    pos = j + 1;
                    break;
                }
            }

            sum += pos;
        }
    }

    return sum;
}

int main(int argc, char const *argv[]) {
    vector<int> a;
    vector<vector<int>> b;

    a.push_back(1);
    a.push_back(0);
    b.push_back(a);

    vector<int> c;
    c.push_back(3);
    c.push_back(3);
    b.push_back(c);

    vector<int> d;
    d.push_back(2);
    d.push_back(2);
    b.push_back(d);

    // for (int i = 0; i < b.size(); i++) {
    //     cout << b[i][0] << ", " << b[i][1] << endl;
    // }

    cout << maxCir(5, 3, b) << endl;

    return 0;
}
