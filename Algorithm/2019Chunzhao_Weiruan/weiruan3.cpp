#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>

using namespace std;

int way_num = 0;
void dfs(int n, int pos_curr, int max_step_curr, int aim) {
    if (max_step_curr < 0) {
        return;
    }

    if (max_step_curr >= 0 && pos_curr == aim) {
        way_num++;
    }

    for (int i = 1; i <= n; i++) {
        if (pos_curr != i && (i % pos_curr == 0 || pos_curr % i == 0)) {
            dfs(n, i, max_step_curr - 1, aim);
        }
    }
}

// intput1: n, input2: pos, input3: max_step
int maxCir(int input1, int input2, int input3) {
    dfs(input1, input2, input3, input2);
    return way_num - 1;
}
int main(int argc, char const *argv[]) {
    cout << maxCir(3, 2, 4) << endl;
    return 0;
}
