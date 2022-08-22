#include <chrono>
#include <iostream>
#include "KThreadPool.h"

void func(int i) {
    std::cout << "func " << i << std::endl;
}

int main() {
    KThreadPool pool(4);
    for (int i = 0; i < 1000; ++i) {
        pool.execute(std::move(func), i);
    }

    return 0;
}