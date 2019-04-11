#include <math.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

double get_distance(int a_x, int a_y, int b_x, int b_y) {
    return sqrt(pow(a_x - b_x, 2) + pow(a_y - b_y, 2));
}

int honey(int input1, int input2, vector<vector<int>> input3,
          vector<vector<int>> input4, vector<int> input5, int input6) {
    vector<bool> is_visit(input1, false);
    int unit = 0;
    double total_distance = 0;
    vector<int> curr_pos;
    curr_pos.push_back(input5[0]);
    curr_pos.push_back(input5[1]);
    while (1) {
        double min_distance = 99999999999999999;
        int flower_pos = 0;
        int honeycomb_pos = 0;
        for (int i = 0; i < input1; i++) {
            if (is_visit[i]) {
                continue;
            }
            double distance_curr = get_distance(curr_pos[0], curr_pos[1],
                                                input3[i][0], input3[i][1]);
            if (min_distance > distance_curr) {
                min_distance = distance_curr;
                flower_pos = i;
            }
        }

        total_distance += min_distance;

        min_distance = 99999999999999999;
        for (int j = 0; j < input2; j++) {
            double distance_curr =
                get_distance(input3[flower_pos][0], input3[flower_pos][1],
                             input4[j][0], input4[j][1]);
            if (min_distance > distance_curr) {
                min_distance = distance_curr;
                honeycomb_pos = j;
            }
        }

        total_distance += min_distance;

        double back_home_distance =
            get_distance(input4[honeycomb_pos][0], input4[honeycomb_pos][1],
                         input5[0], input5[1]);

        if (back_home_distance > (double)input6 - total_distance) {
            break;
        }

        is_visit[flower_pos] = true;
        unit++;
        curr_pos.clear();
        curr_pos.push_back(input4[honeycomb_pos][0]);
        curr_pos.push_back(input4[honeycomb_pos][1]);
    }

    return unit;
}

int main(int argc, char const* argv[]) {
    vector<int> a;
    vector<vector<int>> a3;
    vector<vector<int>> a4;
    vector<int> a5;

    // a3
    a.push_back(3);
    a.push_back(5);
    a3.push_back(a);
    a.clear();

    a.push_back(7);
    a.push_back(5);
    a3.push_back(a);
    a.clear();

    a.push_back(7);
    a.push_back(3);
    a3.push_back(a);
    a.clear();

    a.push_back(8);
    a.push_back(4);
    a3.push_back(a);
    a.clear();

    // a4
    a.push_back(7);
    a.push_back(7);
    a4.push_back(a);
    a.clear();

    // a.push_back(6);
    // a.push_back(1);
    // a4.push_back(a);
    // a.clear();

    a.push_back(5);
    a.push_back(5);

    // for (int i = 0; i < a3.size(); i++) {
    //     cout << a3[i][0] << ", " << a3[i][1] << endl;
    // }

    // for (int i = 0; i < a4.size(); i++) {
    //     cout << a4[i][0] << ", " << a4[i][1] << endl;
    // }

    cout << honey(4, 1, a3, a4, a, 22) << endl;
    return 0;
}
