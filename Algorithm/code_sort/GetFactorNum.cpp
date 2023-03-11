/*
依图面试算法题：
给定一个数字a（1-32768）将其因数分解，要求：
a = a1 * a2 * a3 * a4 * ... * an
a1 <= a2 <= a3 <= a4 <=.... <= an
求能够分解出这样的序列有多少个.

例如:   16 -> 5
        20 -> 4
*/

#include <cmath>
#include <iostream>
#include <vector>
using namespace std;

// 递归的方法
int get_factor_num(int start, int a) {
    int result = 1;
    for (int i = start; i <= sqrt(a); i++) {
        if (a % i == 0 && a / i >= i) {
            result += get_factor_num(i, a / i);
        }
    }
    return result;
}

// 非递归的方法
int get_factor_num_non(int start, int a) {
    vector<vector<int>> dp(a + 1, vector<int>(a + 1, 1));
    for (int j = 2; j <= a; j++) {
        for (int i = 2; i < j; i++) {
            for (int k = i; k <= sqrt(j); k++) {
                if (j % k == 0 && j / k >= k) {
                    dp[i][j] += dp[k][j / k];
                }
            }
        }
    }
    return dp[2][a];
}

int main() {
    for (int i = 2; i < 100; i++) {
        int count1 = get_factor_num(2, i);
        int count2 = get_factor_num_non(2, i);
        printf("The result of %d is: %d\n", i, abs(count1 - count2));
    }

    system("pause");
}
