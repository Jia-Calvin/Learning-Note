#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char const *argv[]) {
    int T;
    cin >> T;
    for (int i = 0; i < T; i++) {
        vector<int> is_exists;
        int n;
        cin >> n;
        vector<int> ids;
        for (int j = 0; j < n; j++) {
            int id;
            cin >> id;
            bool is_exist = false;
            for (int kk = 0; kk < is_exists.size(); kk++) {
                if (is_exists[kk] == id) {
                    is_exist = true;
                }
            }
            if (!is_exist) {
                ids.insert(ids.begin(), id);
                is_exists.push_back(id);
            } else {
                for (int k = 0; k < ids.size(); k++) {
                    if (ids[k] == id) {
                        ids.erase(ids.begin() + k);
                        ids.insert(ids.begin(), id);
                        break;
                    }
                }
            }
        }
        for (int k = 0; k < ids.size(); k++) {
            cout << ids[k] << " ";
        }
        cout << endl;
    }

    // system("pause");
    return 0;
}

// 1 2 3 4 5
// 1 100 1000 1000 100 1
// 1 6 3 3 1 8 1