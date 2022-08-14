#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

void create_vector() {
    vector<float> tmp1(65536, 0);
    vector<float> tmp2(65536, 0);
    vector<float> tmp3(65536, 0);
    vector<float> tmp4(65536, 0);
    vector<float> tmp5(65536, 0);
    vector<float> tmp6(65536, 0);
    vector<float> tmp7(65536, 0);
    vector<float> tmp8(65536, 0);
}

int main(int argc, char const* argv[]) {
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

    create_vector();

    return 0;
}
