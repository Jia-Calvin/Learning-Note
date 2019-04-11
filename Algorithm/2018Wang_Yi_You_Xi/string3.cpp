#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

int get_num_string(vector<vector<char>> matrix, int m, int n, int x, int y,
                   char *str, int length) {
    int result = 0;
    int tmp_x = x, tmp_y = y;
    for (int i = 0; i < length; i++) {
        if (tmp_x >= m || tmp_y >= n) {
            break;
        }

        if (matrix[tmp_x][tmp_y++] != str[i]) {
            break;
        }
        if (i + 1 == length) {
            result++;
        }
    }

    tmp_x = x, tmp_y = y;
    for (int i = 0; i < length; i++) {
        if (tmp_x >= m || tmp_y >= n) {
            break;
        }
        if (matrix[tmp_x++][tmp_y] != str[i]) {
            break;
        }
        if (i + 1 == length) {
            result++;
        }
    }

    tmp_x = x, tmp_y = y;
    for (int i = 0; i < length; i++) {
        if (tmp_x >= m || tmp_y >= n) {
            break;
        }
        if (matrix[tmp_x++][tmp_y++] != str[i]) {
            break;
        }
        if (i + 1 == length) {
            result++;
        }
    }

    return result;
}

int main(int argc, char const *argv[]) {
    int T;
    cin >> T;
    for (int i = 0; i < T; i++) {
        int m, n;
        cin >> m >> n;
        char *str = (char *)malloc(sizeof(char) * max(m, n));

        vector<char> row(n);
        vector<vector<char>> matrix(m, row);

        for (int j = 0; j < m; j++) {
            cin >> str;
            for (int k = 0; k < n; k++) {
                matrix[j][k] = str[k];
            }
        }
        cin >> str;
        int str_length = 0;
        for (int l = 0; l < max(m, n); l++) {
            if (str[l] != '\0') {
                str_length++;
            } else {
                break;
            }
        }

        int result = 0;
        for (int j = 0; j < m; j++) {
            for (int k = 0; k < n; k++) {
                if (matrix[j][k] == str[0]) {
                    result +=
                        get_num_string(matrix, m, n, j, k, str, str_length);
                }
            }
        }
        free(str);
        cout << result << endl;
    }

    system("pause");
    return 0;
}
