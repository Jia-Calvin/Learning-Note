#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

class Solution {
   public:
    void replaceSpace(char *str, int length) {
        if (str == NULL || length <= 0) {
            return;
        }

        int space_num = 0;
        for (int i = 0; i < length; i++) {
            if (str[i] == ' ') {
                space_num++;
            }
        }

        int tmp_len = length + 2 * space_num;
        char *tmp = (char *)malloc(sizeof(char) * (tmp_len + 1));
        tmp[tmp_len] = '\0';
        int k = tmp_len - 1;
        for (int i = length - 1; i >= 0; i--) {
            if (str[i] == ' ') {
                tmp[k--] = '0';
                tmp[k--] = '2';
                tmp[k--] = '%';
            } else {
                tmp[k--] = str[i];
            }
        }

        for (int i = 0; i < tmp_len; i++) {
            str[i] = tmp[i];
        }

        free(tmp);
    }
};

int main(int argc, char const *argv[]) {
    char *str = "daaasdv ddsd.d d  .as ,da  s";
    int str_len = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        str_len++;
    }

    Solution *s = new Solution();
    s->replaceSpace(str, str_len);
    cout << str << endl;

    return 0;
}
