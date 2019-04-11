#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;
long long powers[100];
int costs[100];
int ans = 10000;
int n;

void dfs(long long current_power, int cur_indx, int cost) {
    if (cur_indx == n) {
        ans = min(ans, cost);
        return;
    } else if (cost > ans) {
        return;
    } else {
        if (current_power < powers[cur_indx]) {
            dfs(current_power + powers[cur_indx], cur_indx + 1,
                cost + costs[cur_indx]);
        } else {
            dfs(current_power + powers[cur_indx], cur_indx + 1,
                cost + costs[cur_indx]);
            dfs(current_power, cur_indx + 1, cost);
        }
    }
}

int main(int argc, char const* argv[]) {
    cin >> n;
    for (int i = 0; i < n; i++) {
        cin >> powers[i];
    }

    for (int i = 0; i < n; i++) {
        cin >> costs[i];
    }

    dfs(0, 0, 0);
    cout << ans << endl;

    return 0;
}