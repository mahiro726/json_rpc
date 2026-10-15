#include <iostream>
#include <future>
#include <thread>
#include <memory>

int Add(int num1, int num2)
{
    std::cout << "into add!" << std::endl;
    return num1 + num2;
}

int main()
{
    std::promise<int> pro;

    std::future<int> res = pro.get_future();

    std::thread thr([&pro](){
        int sum = Add(11, 22);
        pro.set_value(sum);
    });

    int sum = res.get();
    thr.join();
    
    std::cout << "sum = " << sum << std::endl;
}