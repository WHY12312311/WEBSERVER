## 1. 流程记录



## 2. 知识点记录

### reinterpret_cast

​	**reinterpret_cast<type-id>(exp)允许将任意类型的exp转换为任意类型的type-id**，在进行转换的过程中使用的是逐个比特复制的方法，所以能够进行任意的转换。该转换方法并不在类的层次中进行，即并不关心继承体系，所以其是有一定风险的。

### std::tuple

​	**tuple即元组，实际上就是泛化的std::pair，即将多个数据方便地合成一个单一对象。**由于tuple中的数据都是没有命名的，所以要使用下表来进行取用。这时候就需要用到std::get函数了。**std::get<N>(t)用于取出tuple变量t中的第N个元素**。

​	也可以将tuple元素解包。使用的函数是std::tie()函数，使用方法如下：

```C++
{ // std::tie: function template, Constructs a tuple object whose elements are references
  // to the arguments in args, in the same order
  // std::ignore: object, This object ignores any value assigned to it. It is designed to be used as an
  // argument for tie to indicate that a specific element in a tuple should be ignored.
    
    int myint;
    char mychar;
 
    std::tuple<int, float, char> mytuple;
 
    mytuple = std::make_tuple(10, 2.6, 'a');          // packing values into tuple
 
    std::tie(myint, std::ignore, mychar) = mytuple;   // unpacking tuple into variables
 
    std::cout << "myint contains: " << myint << '\n';
    std::cout << "mychar contains: " << mychar << '\n';
}
```

​	其余函数直接随用随查就好了。

### std::optional

​	std::optional相当于一个包含bool值的元素。比如声明一个类型std::optional<T>的变量，那么**这个变量内部要么就是T类型，要么是std::nullopt类型，即代表一个空指针**。使用std::optional可以在融合bool思维的前提下减小变量的大小与使用方便。

### getopt函数

```c
int getopt(int argc, char * const argv[], const char *optstring);
// 前两个参数就是argc和argv，第三个参数是选项字符串
```

​	optstring变量是一个字符串，代表的是**输入参数的格式**，这个字符串中每一个字符都代表着一个选项。对每一个字符来说：

+ 没有冒号就代表着纯选项，不需要任何参数
+ 一个冒号就代表着选项之后必须带有参数，其中可以有空格也可以不适用
+ 两个冒号代表该项之后的参数可写可不写

​    其中使用方法要使用while循环。**每次循环地返回值都是当前检测到的选项char字符，而选项后面的参数被放到了定义好的optarg中**。如果出错则返回-1。

### explicit关键字

​	在C++中，可以使用explicit关键字来修饰**只有一个参数的类构造函数**，表示这个构造函数必须是显式的，不能有隐式自动转换的情况发生。比如下述情况：

```C++
class CxString  // 使用关键字explicit的类声明, 显示转换  
{  
public:  
    char *_pstr;  
    int _size;  
    explicit CxString(int size) {}
}; 
    // 下面是调用:  
    CxString string1(24);     // 这样是OK的  
    CxString string2 = 10;    // 这样是不行的, 因为explicit关键字取消了隐式转换  
```

​	对于上面的第二个调用，如果没有声明explicit关键字，即构造函数是隐式的，则可以正确调用，也就是相当于调用了CxString string1(10); 即自动地发生了转换。而当声明了explicit关键字之后，就杜绝了这种自动转换，也就只能使用第一种调用方法了。 

### std::function

​	该关键字**是一个可调用对象包装器，是一个类模板，可以容纳除了类成员函数指针之外的所有可调用对象，它可以用统一的方式处理函数、函数对象、函数指针，并允许保存和延迟它们的执行**。大多数情况下，使用std::function来定义变量可以代替函数指针的使用。其大致用法如下：

```C
# include <iostream>
# include <functional>

typedef std::function<int(int, int)> comfun;

// 普通函数
int add(int a, int b) { return a + b; }

// lambda表达式
auto mod = [](int a, int b){ return a % b; };

// 函数对象类
struct divide{
    int operator()(int denominator, int divisor){
        return denominator/divisor;
    }
};

int main(){
	comfun a = add;
	comfun b = mod;
	comfun c = divide();
    std::cout << a(5, 3) << std::endl;
    std::cout << b(5, 3) << std::endl;
    std::cout << c(5, 3) << std::endl;
}
```



