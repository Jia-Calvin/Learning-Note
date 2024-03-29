## C/C++语言
---

*   ### **new和malloc的区别？**
    -   new是一种操作符(对象内默认的操作符)，malloc是一个向操作系统申请内存的函数
    -   new申请内存返回的类型是对象类型的指针（与对象匹配），而malloc只是单纯地被通知去申请多大的内存块它的返回是无类型指针、需要强制类型转换
    -   new和delete（操作符可重载）会调用对应的构造函数以及析构函数完成对象的创建与析构，malloc和free根据指针申请与释放内存块
    -   new具体步骤：
        -   调用operator new分配足够的内存空间
        -   调用对象的构造函数初始化
        -   构造完毕，返回该对象的类型指针
    -   malloc具体步骤：
        -   申请给定参数的内存块
        -   返回指针指向内存块开始
---

*   ### **placement new/delete、operator new/delete的区别？他们之间有什么关联？**
    - operator new 和 operator delete
        -   operator new 和 operator delete 是内存的分配操作符和归还操作符，编译器会默认添加global operator new 以及 operator delete供使用，为的是正确地申请、释放对象的内存
        -   在一些情况下，我们对于某个class可能会有特定的需求去执行内存的分配和释放（例如执行一些审计、统计等场景），因此c++允许我们使用定制化的operator new 和 operator delete
        -   operator new 和 operator delete就是正常签名式的操作符：
            ``` c++
            // global
            void* operator new(std::size_t) throw(std::bad_alloc);
            // global
            void* operator delete(void* rawMemory) throw();
            // class内专属定义
            void* operator delete(void* rawMemory, std::size_t size) throw();
            ``` 
            -   每个class可以编写自己的operator new+delete覆盖global域中的operator new+delete
            -   必须与正常签名式相符
    
    - placement new 和 placement delete
        -   与正常签名式不同，可以根据定制者传入自己需要的参数，非标准签名式，这些new/delete就是placement的，一个例子：
            ``` c++
            void* operator new(std::size_t, std::ostream& longStream) throw(std::bad_alloc);
            ```
        -   当有这些自己定制化的placement new时，也必须为这个class定义placement delete + operator delete：
            ``` c++
            // placement delete，异常下自动调用
            void* operator delete(std::size_t, std::ostream& longStream) throw(std::bad_alloc);
            // class 专属定义，正常delete时调用
            void* operator delete(void* rawMemory, std::size_t size) throw();
            ```
            -   一一对应是为了在异常场景下（正常申请内存、构造函数调用抛出异常）能够进行清理与恢复
            -   placement delete只会在异常场景被调用，而operator delete是在正常清理下时被调用的，因此一般这两个都需要被实现


---

*   ### **c++中class和struct的主要区别？**
    -   默认继承权限：class是private，struct是public
    -   默认成员函数访问权限：class是private，struct是public
---

*   ### **cout和printf区别**
    -   cout定义在iostream中的一种对象类型，printf是定义在stdio.h头文件中的一种函数
    -   cout更安全，内含大量的运算符重载，可以输出各种基本数据类型，printf需要严格定义的数据类型才可以对应输出
    -   cout不是线程安全的，因为是一个流式的输出，而printf是线程安全的函数，支持多线程下依然能够正常打印日志。
---

*   ### **sizeof和strlen的区别**
    -   sizeof类似于new一样也是操作符，strlen是函数
    -   sizeof的操作对象可以是变量、函数、表达式、数据类型、对象等等，而strlen的传入参数是char*指针，它主要计算字符串长度，遇到结束标识符时返回其长度。
    -   sizeof编译已经确定好要返回的大小，strlen是在运行时计算字符串长度。
---

*	### **static 静态函数、静态全局变量、静态局部变量、静态成员变量、静态成员函数**
	-	当static修饰一个函数时（**静态函数**，面向过程）
    	-	函数只可以被本文件的其余函数调用，不可以跨文件调用
    	-	其他文件有相同函数名时，不会发生冲突

	-	当static修饰一个全局变量时（**静态全局变量**，面向过程）
    	-	静态全局变量只对本文件上有效，其余文件即使用了extern也无法引用静态全局变量
    	-	与其余文件有着相同变量的全局变量不会发生冲突，多个文件可同时定义相同static变量
        -   静态全局变量放置在头文件时，每个文件include都会定义一次，会造成每个文件都有自己定义的变量，而不是全局唯一一个变量，当把static去掉，则会有multiple definition编译错误

	-	当static修饰一个局部变量时（**静态局部变量**，面向过程）
    	-	静态局部变量与静态全局变量是存储在进程的数据区(已初始化的变量存储在.data段、未初始化的变量存储在.bss段)
    	-	静态局部变量只在作用域内有效，出了作用域是无效的，但其始终驻留在数据区直到程序结束
    	-	初始化只在第一次有效，其余无效

    -   当static修饰一个成员变量时（**静态成员变量**，面向对象）
        -   这个变量是所有对象所共有的。普通变量每个对象实例都有自己的一份拷贝，而静态成员变量不是的，所有对象实例共享一份。
        -   静态成员变量同样存储在数据区，内存在初始化时才分配，静态成员变量必须初始化，而且只能在类体外进行初始化，若在类内初始化，则会导致每个对象都包含该静态成员变量，这是矛盾的。
        -   静态成员变量不需要通过某个实例访问，直接访问这个类::变量即可（公有的才可以，私有的必须通过内部的静态函数get才行）

    -   当static修饰一个成员函数时（**静态成员函数**，面向对象）
        -   静态成员函数属于类的本身，不属于任何一个实例对象，不具备this指针。**因此不能够访问非静态成员函数与非静态变量，只能访问静态成员函数与静态变量**。
        -   静态成员函数与静态成员变量一样可以通过类::函数，类::变量直接访问。
---

*	### **谈一谈volatile关键字**

	-	阻止编译器为了提高速度将一个变量缓存到寄存器内而不写回，读取某个值到寄存器操作后必须写回变量里
	-	阻止编译器调整操作volatile变量的指令顺序，即使能够阻止编译器优化，也无法阻止CPU动态调度
		-	这里可以延伸去讲singleton（如何创建单例模式的过程）
			-	new的第二和第三个过程因为CPU的动态调度被交换：
				-	分配内存
				-	调用构造函数
				-	返回内存地址地址给用户
			``` c++
			#define barrier() ___asm___ volatile ("lwsync")
			volatile T* pInst = 0;
			T* GetInstance(){
				if (pInst == NULL) {
					lock();
					if (pInst == NULL) {
						T* temp = new T;
						barrier(); // 建立水坝，cpu动态调度无法穿越这个水坝进行乱序执行
						pInst = temp;
					}
					unlock();
				}
				return pInst;
			}
			```
---

*   ### **详细说说虚函数、纯虚函数的底层实现以及应用**
    *   在声明了虚函数的类里，每个实例都会有一个虚函数指针，对应于其定义的虚函数的地址列表。子类继承含有虚函数的父类时，其也会有虚函数指针。
        *   子类也声明虚函数覆盖父类的虚函数时，编译器会根据**引用或指针**指向**对象类型**的对象寻找其要调用的函数。
        *   子类并未申明虚函数覆盖父类的虚函数时，编译器会根据**引用或指针本身的类型**寻找其要调用的函数。

        ```c++
            //举个例子
            class A {
                public:
                    virtual void func1() { cout << "A: func1" << endl; }
                    virtual void func2() { cout << "A: func2" << endl; }
                };

            class B : public A {
            public:
                virtual void func1() { cout << "B: func1" << endl; }
                //           func2() 未定义
            };

            int main(int argc, char const* argv[]) {
                A* a = new B();
                a->func1();  // 根据指针指向的对象类型即B类型调用，因此调用子类函数
                a->func2(); // 根据指针本身类型即A类型调用，因此调用父类函数
                return 0;
            }

            输出结果：
                B: func1
                A: func2
        ```
    	<div align=center>
        <img src="../pictures/vptr.png" width = "500" height = "450" alt="图片名称" />
		</div>

        *   从上面的图中也可以看出子类func2因为没有覆盖父类的虚函数，因此子类实例的虚函数指针指向的是父类的func2函数的地址。
        *   而子类func1覆盖了父类的虚函数，因此子类实例的虚函数指针指向了自身定义的虚函数。（多态）

    *   通过类本身变量调用函数，是不会通过虚函数指针的，而是会根据本身的类型直接调用函数。
        ``` c++
            class A {
            public:
                virtual void func1() { cout << "A: func1" << endl; }
                virtual void func2() { cout << "A: func2" << endl; }
            };

            class B : public A {
            public:
                virtual void func1() { cout << "B: func1" << endl; }
            };

            int main(int argc, char const* argv[]) {
                B b;
                A a = b;
                a.func1();    // 直接调用类型A的func1函数
                a.func2();    // 直接调用类型A的func2函数
                return 0;
            }

            输出结果：
                A: func1
                A: func2
        ```

    *   纯虚函数定义的类是一种抽象类，不能被实例化的类，无法通过new得到它的实例。
        *   纯虚函数的定义就是用来被子类具体化的，子类中的虚函数必须覆盖纯虚类，对其具体地实现。
        *   纯虚函数必须等于0，不能对这个函数进行任何定义。

        ```c++
            //举个例子
            class A {
                public:
                    virtual void func3() = 0; // 不能具体定义，需 = 0
                };

            class B : public A {
            public:
                virtual void func3() { cout << "B:func3" << endl; }
            };

            int main(int argc, char const* argv[]) {
                A* a = new B();
                a->func3();
                return 0;
            }

            输出结果：
                B: func3
        ```
---

*   ### **析构函数在什么情况下需要被设置为虚函数？**
    -   存在继承关系时，base class（多态基类）的析构函数必须为虚函数
        ```c++
        Base    *pk1 = new Derived();
        Derived *pk2 = new Derived();
        ...
        // 释放内存
        // 若析构函数不为虚函数，则内存泄漏
        delete pk1;
        // 无论如何都正常析构，不会造成内存泄漏
        delete pk2;
        ```
        -   在delete pk2的场景下，析构函数（非虚函数）是这样执行：
            -   调用子类的析构函数
            -   沿着继承链层层调用父类的析构函数
        -   在delete pk1的场景下，析构函数（非虚函数）是这样执行：
            -   直接调用父类的析构函数
            -   没有更上层的父类了，停止调用
        -   在delete pk1的场景下，析构函数（虚函数）是这样执行：
            -   根据虚函数特性，父类指针，但子类实现，因此调用子类的析构函数
            -   沿着继承链层层调用父类的析构函数
            -   本质上是利用虚函数特性调用子类的析构函数，从而正常析构（回到一般化）
    -   不存在继承关系时，class的析构函数不必为虚函数
        -   成员函数被设置为虚函数是有代价的，会导致对象的体积膨胀（虚函数指针、虚函数表等）
        -   毫无意义的开销，并且会有可能破坏代码的移植性

---

*   ### **为什么不要让析构函数抛出异常？抛出异常会有什么后果？**
    -   让析构函数抛出异常会导致隐式的资源泄漏，实际上并未完整地清理干净
        -   例如清理一个数组指针，指针指向实际内容，在清理第2个的时候抛异常了，导致内存泄漏
        -   例如清理数组的网络链接，指针指向实际的fd，在清理第2个的时候抛异常了，导致链接泄漏
    -   正确的做法应该是：1）捕获异常继续执行清理；2）捕获异常退出进程；3）改变设计，避免在析构函数吐出异常

---

*   ### **为什么不要在构造和析构函数中调用virtual函数？**
    -   父类构造函数执行过程，C++绝不允许虚函数的调用下降到子类上去
        -   子类还未构建，私有变量都未初始化，下降是非常危险的动作，C++不允许这么做
        -   父类构造过程中，C++会把该变量视作父类的类型，而不是子类的类型
    -   子类析构函数执行过程，C++绝不允许虚函数的调用下降到子类上去
        -   析构较难解释，但道理和构造过程是类似的
        -   子类一旦开始析构，私有变量都会被视为未定义类型，调用虚函数也不会到达子类
        -   进入父类的析构过程时，类型就会转换为父类类型
    -   构造和析构过程，虚函数不是虚函数！！

---

*   ### **谈谈C++11的新的类型转换？**
    -   const_cast
        -   将const类型强制转换为非const类型
    -   dynamic_cast
        -   动态转换类型，转换前会做检查，“安全的向下转型”
        -   常用于父类向子类转换的过程，开销很高，尽量少用，保持猜疑
        -   可以从设计中避免此类转换，例如
            -   使用智能指针保存子类的实现
            -   使得需要调用的方法作为虚函数，即父类也存在该函数
    -   static_cast
        -   静态类型转换，各种基本类型的转换，例如int、long、double、float等转换
        -   非const到const的转换，但不支持反向转换，反向只有const_cast支持
    -   reinterpret_cast
        -   很少用，是很底层的类型转换



---

*   ### **C语言中的memcpy与memset对class操作造成什么后果**

    *   `void* memset(void* s, int ch, size_t n)` 对 class 初始化时：
        *   若class内部包含虚函数，memset会将虚函数指针设置为0，若后面继承用到这个类的虚函数指针时，会使程序崩溃。
        *   若class内部包含vector<>, stack<>等STL模板类，memset会将所有的类的变量设置为0，破坏了模板类的内部结构，导致指针指向内存错误或是访问越界等情况，使得程序崩溃。

    *   `void* memcpy(void* dest, const void* src, unsigned int count)`对class的复制时：
        *   若class内部包含虚函数，memcpy会将这个类的虚函数指针地址也同时复制，这种复制是有效的，因为复制类本身就要求完全相同。
        *   若class内部包含指向其他类的指针，这种拷贝是一种浅拷贝，只会复制指向这个类的指针，而不会将指向类的内存再复制一份，若需要深拷贝则需要在构造函数里在申请空间。
---

*   ### **extern "C"的作用**

    *   C++语言的函数重载是通过由编译器将函数（与参数）进行唯一的符号修饰，在链接过程中才不会发生符号冲突。
    *   C语言不存在函数重载，因此不存在符号的修饰。直接通过C++调用C语言的函数时会存在链接错误的问题，寻找不到这个函数的错误。
    *   extern "C"就是用来解决这种不兼容问题，extern "C" 包裹的代码块内的所有变量以及函数，不会被g++编译器进行符号修饰，而依旧用原来的gcc编译的方法进行符号修饰。
    *   但是C语言不兼容extern "C"，所以函数无法同时被C和C++都使用，此时可在头文件将函数声明为，因C++编译器会在编译c++程序时默认定义宏__cplusplus：
        ```c++
        #ifdef __cplusplus
        extern "C"{
        #endif

        void func();

        #ifdef __cplusplus
        }
        #endif
        ```
    *   符号修饰是为了使不同的代码语言例如汇编以及C语言产生的目标文件能够正常使用定义的符号，各自语言的符号修饰使得定义的变量与函数不会互相冲突。
---

*   ### **强符号与弱符号，强引用与弱引用**

    *   强符号与弱符号：
        -   强符号：编译器默认函数与初始化了的全局变量
        -   弱符号：未初始化的全局变量
        -   规则：
            -   不允许强符号被多次定义，例如多个文件中同时定义相同的初始化的全局变量。
            -   若一个文件中的某个符号为强符号（初始化），其余文件中的相同符号为弱符号（未初始化），编译器会选择强符号进行链接。
            -   所有文件都是弱符号，选择占用空间最大的一个。例如某个符号有 int 与 double，那么编译器会选择double。（不要使用！）

    *   强引用与弱引用：
        -   强引用：外部符号引用需要被正确决议，如果无法找到这个符号，则链接器报错
        -   弱引用：外部符号引用无需立刻被决议，若有定义，则正确决议，若无定义，链接器不报错，但运行时还未被决议（反汇编的函数与变量地址全为0），则运行时报错。

    *   弱引用与弱符号主要用于库的链接过程
        -   库中定义的弱符号可以被用户定义的强符号覆盖，例如printf
        -   将定义的外部引用定义为弱引用，编译通过，当将外部模块与程序链接时，程序得以正常运行，外部模块也可以随时切换（松耦合）
---

*   ### **右值和左值的概念**

    * 左值指既能够出现在等号左边，也能出现在等号右边的变量（赋值与被赋值）。
    * 右值则是只能出现在等号右边的变量（赋值）。
    * 左值一般有变量名字，通过变量的名字能够索引到相应的内存块。
    * 右值一般指临时变量，这些临时变量是编译器自动产生的，无法在代码中直接地通过变量名索引内存块。
    * 左值和右值主要的区别之一是左值可以被修改，而右值不能。
    * **右值和左值示例如下**：
        ```c++
        int a; // a 为左值
        a = 3; // 3 为右值
        ```
    
---

*   ### **右值引用和左值引用的概念**

    * 左值引用：必须引用一个对象，以前在C++使用的T&（引用）一般就是C++11就被称为左值引用。
    * 右值引用：就是必须绑定到右值的引用，C++11中右值引用可以实现“移动语义”，通过 && 获得右值引用。

    **右值引用和左值引用示例如下**：
    ```C++
    int x = 3;
    int& y = x; // ok，y为左值引用，绑定在对象x上。

    int& a = x * 6; // 错误，x*6是一个右值，不能够通过左值引用修改右值，违背语义
    const int& b =  x * 6; // 正确，可以将一个const引用绑定到一个右值，这样便无法通过左值引用修改右值，合法！

    int&& q = 3; // ok，q为右值引用，可以绑定在右值上
    int&& w = x; // 错误，x是一个左值
    ```
    在上面的例中，可以看到const左值引用能够引用右值。那么能否有右值引用能够引用左值呢？ std::move，std::forward就是为了做这类事情而诞生的。
---

*   ### **std::move的作用**

    * https://zhuanlan.zhihu.com/p/335994370, 可以参考～！
    * std::move本质不会移动任何东西，做的事情只有**强制类型转换**，将左/右值都转换为右值引用。
    * 单纯的std::move(xxx)不会有任何的性能提升，必须配合**移动语义**以避免在某些情况产生的拷贝。
    * 形参使用 左值引用（T&&）或者 右值引用（T&）都能够避免传参数时的拷贝，性能是一样的。
    * 右值引用可以直接指向右值，也可以通过std::move指向左值；而左值引用只能指向左值(const左值引用也能指向右值，但不能够通过左值引用修改右值)。
    * 真正能够避免拷贝的过程是由**移动构造函数**、**移动复制构造函数**、**移动赋值运算符重载函数**的实现决定的，举一个例子：
        ```c++
        class A {
        private:
            std::string _data;

        public:
            // 一般构造函数，主要针对左值引用（形参）与左值（实参），传参过程无拷贝
            // ！！注意！！ data形参本身是左值，能够放在等号左边
            explicit A(const std::string &data) {
                std::cout << "genernal construct" << std::endl;
                _data = data;  // 会产生std::string临时变量的一次拷贝
            };

            // 移动构造函数，主要针对右值引用（形参）与右值（实参），传参过程无拷贝
            // ！！注意！！ data形参本身也是左值，能够放在等号左边
            explicit A(const std::string &&data) {
                std::cout << "move construct" << std::endl;
                // 这里必须调用std::move，将左值转成右值引用，否则无法调用移动复制构造函数
                // 是否产生拷贝取决于std::string的移动复制构造函数实现
                _data = std::move(data); 
            };
        };
        ``` 
    * 一般STL的各类容器都会内置实现**移动构造函数**、**移动复制构造函数**、**移动赋值运算符重载函数**，因此能够使用右值引用的方式则尽可能使用，能够提升一定的性能
---

*   ### **std::move和std::forward**
    * std::forward功能比std::move更强大，std::move只能够转换出右值引用。
    * std::forward能够自动区分传递的实参是左值还是右值，根据不同的值类型，作出完美的转发。
---

*   ### **拷贝（复制）构造函数**

    * 传递类本身的参数时必须是引用类型：```A (const A& a)```
    * 传值，将引起拷贝过程，无限循环调用拷贝构造函数，因此编译器会直接禁用：```A (const A a)```
---

*   ### **赋值运算符重载函数**
    * 必须返回类的引用类型：```A& A::operator= (const A& other)```，只有返回引用类型，才可以有连等的表达式：```A = B = C```。
    * 必须判断参数other是否为本身，是的话应该直接返回，不做任何变更（边界条件）。
    * 申请内存时，应先用临时变量申请，后变更自身的状态，否则内存不足时抛异常，类实例状态不一致，容易导致程序崩溃。
---


*   ### **C++11 weak_ptr的作用**
    -   weak_ptr被设计出来其实完全是给shared_ptr服务的，是一种辅助指针，主要解决以下两个场景：
        -   为了解决shared_ptr可能存在循环引用的场景，程序中shared_ptr若存在循环引用，计数器始终没办法减为0，因此无法正确调用析构函数，造成内存泄漏
        -   为了解决某些场景无法准确地判断shared_ptr内对象的生命周期，使用weak_ptr可以安全地访问对象，例如在访问前能够通过`expired()`判断对象是否已经被析构，也能够通过`lock()`方法获取对象的shared_ptr
    <br/>   
    -   举一个循环引用的例子：
        ```c++
        #include <memory>

        class B;  // 前向声明

        class A {
        public:
            A() {
                std::cout << "A Constructor" << std::endl;
            }
            ~A() {
                std::cout << "A Destructor" << std::endl;
            }
            std::shared_ptr<B> ptrB;
        };

        class B {
        public:
            B() {
                std::cout << "B Constructor" << std::endl;
            }
            ~B() {
                std::cout << "B Destructor" << std::endl;
            }
            std::shared_ptr<A> ptrA;
        };

        int main() {
            std::shared_ptr<A> ptrA(new A());
            std::shared_ptr<B> ptrB(new B());

            ptrA->ptrB = ptrB; // A 引用 B 
            ptrB->ptrA = ptrA; // B 引用 A

            std::cout << ptrA.use_count() << std::endl; // 引用计数为2
            std::cout << ptrB.use_count() << std::endl; // 引用计数为2

            return 0;
        }
        ```
        -   假设有两个类A和B，A中包含一个shared pointer指向B的对象，而B中也包含一个shared pointer指向A的对象，这就形成了相互循环引用的情况。
        -   在函数退出时，B的引用计数器-1，为1，B不会调用析构函数，因此A的引用计数仍然保持为2，紧接着A的引用计数器-1，为1，也不会调用析构函数，因此最终A与B都不会调用析构函数，造成内存泄漏。
    <br/>
    -   如何解决循环引用问题？修改A或者B其中一个指针类型，例如：
        ```c++
        class B {
        public:
            B() {
                std::cout << "B Constructor" << std::endl;
            }
            ~B() {
                std::cout << "B Destructor" << std::endl;
            }
            // 使用 weak_ptr 规避循环引用问题，weak_ptr不会改变A对象的引用计数器
            // 并且 使用 weak_ptr 有一系列函数提供安全地访问指针
            std::weak_ptr<A> ptrA; 
        };
        ```
        -   这样A的引用计数器在函数未退出前，是1，函数退出后，A对象则自然被析构，因此B的引用计数器最终能到达0，B对象也会被析构，内存泄漏问题解决。
