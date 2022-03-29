#include <iostream>
#include <list>
#include <memory>
#include <stack>

class AssignMyString {
   private:
    std::string _otherData;
    std::unique_ptr<std::string> _data;

   public:
    // 一般构造函数，主要针对左值
    explicit AssignMyString(const std::string &data,
                            const std::string &otherData) {
        // std::unique_ptr<std::string> _tmp =
        //     std::unique_ptr<std::string>(new
        // std::string(std::move(data)));
        // // _data = std::move(_tmp);

        std::cout << "genernal construct" << std::endl;
        _otherData = otherData;  // 无论如何都要拷贝。。
    };

    //
    // 移动构造函数，主要针对右值引用
    explicit AssignMyString(const std::string &&data,
                            const std::string &&otherData) {
        // std::unique_ptr<std::string> _tmp =
        //     std::unique_ptr<std::string>(new std::string(std::move(data)));
        // // _data = std::move(_tmp);

        std::cout << "move construct" << std::endl;
        _otherData = otherData;
    };

    // 复制构造函数
    explicit AssignMyString(const AssignMyString &other) {
        std::cout << "copy construct" << std::endl;
    };

    // 赋值构造函数
    const AssignMyString &operator=(const AssignMyString otherStr) {
        if (this == &otherStr) {
            return *this;
        }
        // std::unique_ptr<std::string> _tmp =
        //     std::unique_ptr<std::string>(new std::string(*otherStr._data));
        // // this->_data = std::move(_tmp);
        std::cout << "assign construct" << std::endl;
        this->_otherData = otherStr._otherData;
        return *this;
    };

    void print() {
        // std::cout << "data is: " << *_data << ", ptr is:" << _data.get()
        //           << std::endl;
        std::cout << "otherData is: " << _otherData
                  << ", ptr is:" << &_otherData << std::endl;
    };

    ~AssignMyString() {
        _otherData = "";
        _data.reset(nullptr);
    }
};

int main(int argc, char const *argv[]) {
    std::string ccc = "123";
    AssignMyString my(std::move(ccc), "456");
    AssignMyString my2("789", "789");
    AssignMyString my3(my);
    // my = my2;

    my.print();
    my2.print();
    my3.print();

    return 0;
}
