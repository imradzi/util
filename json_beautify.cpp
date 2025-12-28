#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <string>

std::tuple<bool, std::string> json_beautify(const std::string compact_json_string, int indent) {
    try {
        nlohmann::json j = nlohmann::json::parse(compact_json_string);
        return {true, j.dump(indent)};
    } catch (const nlohmann::json::parse_error& e) {
        return {false, fmt::format("JSON parse error: {}", e.what())};
    }

    return {false, ""};
}
