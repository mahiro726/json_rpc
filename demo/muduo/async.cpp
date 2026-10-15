#include <iostream>
#include <future>

int Add(int num1, int num2)
{
    return num1 + num2;
}

int main()
{
    // 发起任务，保存之后取得结果的入口。
    std::future<int> res = std::async(std::launch::deferred, Add, 11, 22); // deferred有点像懒汉模式

    int sum = res.get();

    std::cout << "sum = " << sum << std::endl;

    std::cout << "----------------主线程继续执行----------------" << std::endl;
 
}