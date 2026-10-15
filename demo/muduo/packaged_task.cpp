#include <iostream>
#include <future>
#include <thread>
#include <memory>

int Add(int num1, int num2)
{
    return num1 + num2;
}

int main()
{
    // 封装函数，此时尚未执行 Add，好处是我们可以自行决定函数在哪开始执行
    auto task = std::make_shared<std::packaged_task<int(int, int)>>(Add);

    // future对象本质就是结果获取入口
    std::future<int> res = task->get_future();
    
    std::thread thr([task](){
        (*task)(11, 22);
    });

    int sum = res.get();
    thr.join();
    
    std::cout << "sum = " << sum << std::endl;
}