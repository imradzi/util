#pragma once
#include <random>
#include <vector>
#include <memory>

namespace Random {
    template<typename T>
    class Picker {
        std::random_device rd;
        std::mt19937 gen;
        std::vector<T> data;

    public:
        Picker(const std::vector<T> vList) : gen(rd()),
                                             data(vList) {}

        size_t GetSize() { return data.size(); }

        // Get next number from the list and remove it from the list;
        auto GetNextRandom() -> std::unique_ptr<T> {
            if (data.empty()) {
                return nullptr;
            }
            std::uniform_int_distribution<size_t> r(0, data.size() - 1);
            auto it = data.begin();
            std::advance(it, r(gen));
            if (it != data.end()) {
                std::unique_ptr<T> res = std::make_unique<T>(*it);
                data.erase(it);
                return res;
            }
            return nullptr;
        }
    };
}

/*
#include <iostream>

// how to use Random::Picker
int main() {
    Random::Picker<std::string> rp({"abc", "def", "xyz", "ttt", "this is a test"});

    std::string delim="";
    for (auto r = rp.GetNextRandom() ;r != nullptr; r = rp.GetNextRandom()) {
        std::cout << delim << *r;
        delim = ",";
    }
    std::cout << '\n';
}
*/