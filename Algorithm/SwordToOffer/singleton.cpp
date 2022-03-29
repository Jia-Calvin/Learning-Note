#include <iostream>
#include <list>
#include <memory>
#include <stack>

class A {
   private:
    std::string val;

   public:
    A() {
        std::cout << "A construct" << std::endl;
        val = "123";
    };
    ~A() = default;
};

class singleton {
   public:
    singleton() = default;
    ~singleton() = default;

    static A& getInstace() {
        static A a;
        return a;
    }
};

int main(int argc, char const* argv[]) {
    A& a = singleton::getInstace();
    A& b = singleton::getInstace();
    A& c = singleton::getInstace();
    A& d = singleton::getInstace();
    std::cout << "a address: " << &a << std::endl;
    std::cout << "b address: " << &b << std::endl;
    std::cout << "c address: " << &c << std::endl;
    std::cout << "d address: " << &d << std::endl;

    return 0;
}
