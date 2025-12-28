#pragma once
#include <string>
#include <sstream>
template<typename T>
std::string ConvertToUrlEncodedBody(const T& array) {
    std::ostringstream out;

    std::string delim;
    for (const auto& x : array) {
        out << delim << x.first << "=" << x.second;
        delim = "&";
    }
    return out.str();
}
