#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc, char const *argv[]) {
    std::vector<int> vecInt;
    vecInt.push_back(1);
    vecInt.push_back(2);
    vecInt.push_back(2);
    vecInt.push_back(3);
    vecInt.push_back(2);

    // for (int i = 0; i < vecInt.size(); i++) {
    //     std::cout << vecInt[i] << ", ";
    //     if (vecInt[i] == 2) {
    //         vecInt.erase(vecInt.begin() + i--);
    //     }
    //     std::cout << vecInt.size() << std::endl;
    // }

    for (auto itr = vecInt.begin(); itr != vecInt.end(); itr++) {
        std::cout << *itr << ", ";
        if (*itr == 2) {
            vecInt.erase(itr);
        }
        // std::cout << vecInt.end() << std::endl;
        // else {
        //     itr++;
        // }
    }

    return 0;
}
