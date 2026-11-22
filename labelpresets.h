#pragma once

#include <string>
#include <vector>

/// Label size presets shipped as resources/label-presets.json and embedded
/// as a fallback so the dialog always has something to offer.
namespace LabelPresets {

struct Preset {
    std::string name;
    std::string role;   // "label" or "dispensing"
    std::string brand;
    int widthMm {35};
    int heightMm {25};
    int dpi {203};
};

/// Reads one JSON file. Returns the built-in fallback when the file is
/// missing or malformed.
std::vector<Preset> LoadFromFile(const std::string& path);

/// Presets matching `role` ("label" or "dispensing"). An empty role returns
/// every preset.
std::vector<Preset> ForRole(const std::vector<Preset>& presets, const std::string& role);

/// Small built-in list used when the shipped JSON cannot be read.
std::vector<Preset> BuiltIn();

}  // namespace LabelPresets
