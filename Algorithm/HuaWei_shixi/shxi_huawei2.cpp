#include <cmath>
#include <iostream>
using namespace std;

double get_distance(int start_x, int start_y, int stop_x, int stop_y) {
    return sqrt(pow(start_x - stop_x, 2) + pow(start_y - stop_y, 2));
}

int k = 0;
void dfs(int flower_pos[], bool is_visit[], int current_pos, double distance,
         double distance_all[]) {
    int remain_visit = 0;
    for (int i = 0; i < 5; i++) {
        if (is_visit[i] == false) {
            remain_visit++;
        }
    }

    if (remain_visit == 0) {
        distance_all[k++] =
            distance + get_distance(0, 0, flower_pos[2 * current_pos],
                                    flower_pos[2 * current_pos + 1]);
    }

    for (int i = 0; i < 5; i++) {
        if (is_visit[i] == false) {
            is_visit[i] = true;
            double distance_tmp;
            if (current_pos == -1) {
                distance_tmp = get_distance(0, 0, flower_pos[2 * i],
                                            flower_pos[2 * i + 1]);
            } else {
                distance_tmp =
                    get_distance(flower_pos[2 * current_pos],
                                 flower_pos[2 * current_pos + 1],
                                 flower_pos[2 * i], flower_pos[2 * i + 1]);
            }
            dfs(flower_pos, is_visit, i, distance + distance_tmp, distance_all);
            is_visit[i] = false;
        }
    }
}

int main() {
    int flower_pos[10] = {200, 0, 200, 10, 200, 50, 200, 30, 200, 25};

    bool is_visit[5] = {0};
    double distance_all[120] = {0};

    dfs(flower_pos, is_visit, -1, 0, distance_all);

    double min_distance = 99999999999;
    for (int i = 0; i < 120; i++) {
        if (min_distance > distance_all[i]) {
            min_distance = distance_all[i];
        }
        cout << distance_all[i] << ", ";
    }
    cout << endl << min_distance << endl;
    system("pause");
}