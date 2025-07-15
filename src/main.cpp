#include "ZeroCopyCamera.hpp"
#include <iostream>

int main() {
    ZeroCopyCamera camera;
    if (!camera.initialize()) {
        std::cerr << "Camera initialization failed." << std::endl;
        return 1;
    }

    camera.start();
    std::cout << "Camera initialized. Press Enter to exit." << std::endl;
    std::cin.get();

    camera.shutdown();
    std::cout << "Camera shutdown completed." << std::endl;

    return 0;
}
