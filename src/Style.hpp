#pragma once
#include "Classifier.hpp"
#include <Geode/Geode.hpp>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace autodeco {

struct RGB {
    uint8_t r = 0, g = 0, b = 0;
    cocos2d::ccColor3B cc() const { return {r, g, b}; }
};

// A role -> object piece mapping. rotationOffset is added to the classifier's rotation,
// for block families whose reference piece faces a different way than the 1.0 set.
struct Piece {
    int id = 0;
    int rotationOffset = 0;
};

struct ScatterRule {
    int id = 0;
    float density = 0.01f;     // probability per empty grid cell
    float scaleMin = 1.f, scaleMax = 1.f;
    bool randomRotation = false;
    int zLayer = -3;           // ZLayer::B4
    int zOrder = 0;
    int color = 3;             // colour channel
    int minClearance = 0;      // cells of empty space required around the object
};

struct Style {
    std::string id;            // file stem
    std::string name;
    std::string description;
    bool builtin = false;

    // Colour channels. Keys: bg, ground, ground2, line, object, and numbered channels "1".."n".
    std::map<std::string, RGB> palette;

    std::array<std::optional<Piece>, (size_t)Role::COUNT> blocks;
    int blockBaseChannel = 1;
    int blockDetailChannel = 2;

    // Hazard object ID remap (source ID -> replacement ID). Replacement must share the hitbox
    // of the source. An empty map leaves hazard IDs alone and only recolours them.
    std::map<int, int> hazards;
    int hazardBaseChannel = 1;
    int hazardDetailChannel = 2;
    bool recolorHazards = true;

    std::vector<ScatterRule> scatter;

    bool hasBlocks() const { return blocks[(size_t)Role::Inner] && blocks[(size_t)Role::Edge]; }

    // Resolve the piece for a role, falling back to simpler roles if the style lacks one.
    Piece pieceFor(Role role) const;

    static geode::Result<Style> fromJson(matjson::Value const& v, std::string const& id);
    matjson::Value toJson() const;
};

class StyleLibrary {
public:
    static StyleLibrary& get();
    void reload();
    std::vector<Style> const& styles() const { return m_styles; }
    Style const* find(std::string const& id) const;
    std::filesystem::path userDir() const;
    geode::Result<> saveUserStyle(Style const& style);

private:
    std::vector<Style> m_styles;
    void loadDir(std::filesystem::path const& dir, bool builtin);
};

std::optional<RGB> parseColor(matjson::Value const& v);
std::string colorToHex(RGB c);

} // namespace autodeco
