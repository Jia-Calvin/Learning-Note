#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>
using namespace std;

int calcDis(int distance, int loss_town, int deter_town) {
    list<int> list_loss, list_deter;
    list_loss.push_back(loss_town);
    list_deter.push_back(deter_town);
    for (int i = loss_town; i >= 1;) {
        i = i / 2;
        list_loss.push_back(i);
    }

    for (int i = deter_town; i >= 1;) {
        i = i / 2;
        list_deter.push_back(i);
    }

    int diff = list_deter.size() - list_loss.size();
    int cnt_loss = 0, cnt_deter = 0;
    list<int>::iterator iter;

    if (diff >= 0) {
        for (iter = list_deter.begin(); diff > 0; iter++, diff--) {
            list_deter.pop_front();
            cnt_deter++;
        }
    } else {
        diff = abs(diff);
        for (iter = list_loss.begin(); diff > 0; iter++, diff--) {
            list_loss.pop_front();
            cnt_loss++;
        }
    }

    list<int>::iterator iter_loss;
    list<int>::iterator iter_deter;

    for (iter_loss = list_loss.begin(), iter_deter = list_deter.begin();
         iter_loss != list_loss.end(); iter_loss++, iter_deter++) {
        if (*iter_loss == *iter_deter) {
            break;
        }
        cnt_loss++;
        cnt_deter++;
    }

    return (cnt_loss + cnt_deter) * distance;
}

int main(int argc, char const *argv[]) {
    cout << calcDis(4, 2, 3) << endl;
    return 0;
}
