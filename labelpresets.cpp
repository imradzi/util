#include "labelpresets.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace LabelPresets {

std::vector<Preset> BuiltIn() {
    return {
        {"Price tag 25 x 15 mm", "label", "", 25, 15, 203},
        {"Price tag 35 x 25 mm", "label", "", 35, 25, 203},
        {"Price tag 40 x 30 mm", "label", "", 40, 30, 203},
        {"Price tag 50 x 30 mm", "label", "", 50, 30, 300},
        {"Price tag 60 x 40 mm", "label", "", 60, 40, 300},
        {"Price tag 100 x 50 mm", "label", "", 100, 50, 300},
        {"Dispensing 60 x 40 mm", "dispensing", "", 60, 40, 203},
        {"Dispensing 80 x 40 mm", "dispensing", "", 80, 40, 300},
        {"Dispensing 100 x 50 mm", "dispensing", "", 100, 50, 300},
        {"Dispensing 100 x 150 mm", "dispensing", "", 100, 150, 300},
    };
}

std::vector<Preset> LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return BuiltIn();

    try {
        nlohmann::json doc;
        in >> doc;
        const auto& list = doc.contains("presets") ? doc["presets"] : doc;
        if (!list.is_array()) return BuiltIn();

        std::vector<Preset> presets;
        for (const auto& item : list) {
            if (!item.is_object()) continue;
            Preset preset;
            preset.name = item.value("name", std::string {});
            preset.role = item.value("role", std::string {"label"});
            preset.brand = item.value("brand", std::string {});
            preset.widthMm = item.value("widthMm", 0);
            preset.heightMm = item.value("heightMm", 0);
            preset.dpi = item.value("dpi", 203);
            if (preset.name.empty() || preset.widthMm <= 0 || preset.heightMm <= 0) continue;
            if (preset.role != "label" && preset.role != "dispensing") preset.role = "label";
            presets.emplace_back(std::move(preset));
        }
        return presets.empty() ? BuiltIn() : presets;
    } catch (...) {
        return BuiltIn();
    }
}

std::vector<Preset> ForRole(const std::vector<Preset>& presets, const std::string& role) {
    if (role.empty()) return presets;
    std::vector<Preset> result;
    for (const auto& preset : presets) {
        if (preset.role == role) result.emplace_back(preset);
    }
    return result;
}

}  // namespace LabelPresets
