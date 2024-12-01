#include "PluginInterface.hpp"
#include <cstdlib>
#include <cstring>
#include <forward_list>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <dlfcn.h>
#include <iostream>
#include <algorithm>
#include <memory>
#include "Decorator.hpp"
#include "NoiseCreator.hpp"
#include "NoiseReduction.hpp"
#include "LoggingDecorator.hpp"    // Include the LoggingDecorator

typedef PluginInterface* (*create_t)();

std::forward_list<void*> dl_handles;

void writeToCSV(const std::string& filename, const std::vector<double>& data) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& value : data) {
            file << value << "\n";
        }
        file.close();
        std::cout << "Data written to " << filename << std::endl;
    } else {
        std::cerr << "Unable to open file: " << filename << std::endl;
    }
}

std::vector<double> read_csv_values(const std::string& file_name) {
    std::vector<double> data;
    std::ifstream file(file_name);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file " << file_name << std::endl;
        return data;
    }

    std::string line;

    while (std::getline(file, line)) {
        try {
            data.push_back(std::stod(line));
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: Invalid data format in the file. Skipping value: " << line << std::endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "Error: Value out of range. Skipping value: " << line << std::endl;
        }
    }

    file.close();
    return data;
}

void cleanDlHandles() {
    std::for_each(dl_handles.begin(), dl_handles.end(), [](void* e) { dlclose(e); });
}

PluginInterface* getPlugin(const char* libName) {
    void* handle = dlopen(libName, RTLD_LAZY);
    if (!handle) {
        std::cerr << "Cannot open library: " << dlerror() << '\n';
        return nullptr;
    }

    dl_handles.push_front(handle);

    create_t create_plugin = (create_t)dlsym(handle, "createPlugin");
    if (!create_plugin) {
        std::cerr << "Cannot load symbol 'createPlugin': " << dlerror() << '\n';
        return nullptr;
    }

    return create_plugin();
}

int main(int argc, char** argv) {
    PluginInterface* sensor = getPlugin("lib/libSensor.so");
    PluginInterface* noise_creator_plugin = getPlugin("lib/libNoiseCreator.so");
    PluginInterface* denoise_creator_plugin = getPlugin("lib/libNoiseReduction.so");
    atexit(cleanDlHandles);

    if (!sensor || !noise_creator_plugin || !denoise_creator_plugin) {
        std::cerr << "Error loading plugins." << std::endl;
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        try {
            if (!std::strcmp(argv[i], "--help")) {
                std::cerr << "Usage: " << argv[0]
                          << " [OPTIONS]\n\t--min-value NUM\t\tMin temperature value\n\t--max-value NUM\t\tMax temperature value\n\t--num-samples NUM\tNumber of samples to generate\n\t--period NUM\t\tNumber of samples in a day\n\t--range NUM\t\tSensor noise range\n\t--window-size NUM\tWindow size for noise reduction\n";
                return 0;
            } else if (!std::strcmp(argv[i], "--min-value")) {
                sensor->setParameter("min_value", std::stod(argv[++i]));
            } else if (!std::strcmp(argv[i], "--max-value")) {
                sensor->setParameter("max_value", std::stod(argv[++i]));
            } else if (!std::strcmp(argv[i], "--num-samples")) {
                sensor->setParameter("num_samples", (unsigned int)std::stoul(argv[++i]));
            } else if (!std::strcmp(argv[i], "--period")) {
                sensor->setParameter("period", (unsigned int)std::stoul(argv[++i]));
            } else if (!std::strcmp(argv[i], "--range")) {
                noise_creator_plugin->setParameter("range", std::stod(argv[++i]));
            } else if (!std::strcmp(argv[i], "--window-size")) {
                denoise_creator_plugin->setParameter("window_size", (unsigned int)std::stoul(argv[++i]));
            } else {
                std::cerr << "Invalid argument: " << argv[i] << std::endl;
                delete sensor;
                delete noise_creator_plugin;
                delete denoise_creator_plugin;
                return 1;
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid value: " << argv[i] << std::endl;
            delete sensor;
            delete noise_creator_plugin;
            delete denoise_creator_plugin;
            return 1;
        }
    }

    std::vector<double> sensor_data = sensor->execute();
    writeToCSV("sensor_data.csv", sensor_data);

    // Use the dynamically loaded `noise_creator_plugin` directly
    Decorator logging_decorator(noise_creator_plugin);
    logging_decorator.setParameter("input", sensor_data);
    std::vector<double> noised_data = logging_decorator.execute();
    writeToCSV("noise_data.csv", noised_data);

    // Similarly, use `denoise_creator_plugin`
    denoise_creator_plugin->setParameter("input", noised_data);
    std::vector<double> denoised_data = denoise_creator_plugin->execute();
    writeToCSV("denoised_data.csv", denoised_data);

    delete sensor;
    delete noise_creator_plugin;
    delete denoise_creator_plugin;

    return 0;
}
