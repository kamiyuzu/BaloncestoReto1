#ifndef LOGGINGDECORATOR_HPP
#define LOGGINGDECORATOR_HPP

#include "Decorator.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

class LoggingDecorator : public Decorator {
public:

    // Override the execute method to add logging
    std::vector<double> execute() override {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "[LOG] Starting execution...\n";

        // Call the wrapped component's execute method
        std::vector<double> result = component->execute();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "[LOG] Execution finished in " << std::fixed << std::setprecision(3) << elapsed.count() << " seconds.\n";

        return result;
    }
};

#endif // LOGGINGDECORATOR_HPP
