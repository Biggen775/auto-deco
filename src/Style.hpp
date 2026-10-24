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

// A colour channel setting: colour plus optional blending / opacity.
struct Channel {
    RGB color;
    bool blending = false;
    float opacity = 1.f;
};

// Objects placed along exposed block edges: outlines, glow strips, shadows.
// Reference orientation mirrors the block roles: `edge` sits on the TOP edge of the cell,
// `corner` on the TOP-LEFT outer corner, `innerCorner` in a TOP-LEFT concave corner.
// Offsets are in the reference orientation and are rotated with the piece.
struct EdgeLayer {
    std::string name;
    int edge = 0, corner = 0, innerCorner = 0;
    int rotationOffset = 0;
    float offsetX = 0.f, offsetY = 0.f;
    float scale = 1.f;
    float scaleX = 0.f, scaleY = 0.f; // 0 = use `scale` (non-uniform scale for thick lines)
    int zLayer = 3;   // B1
    int zOrder = 0;
    int color = 2;
    int detailColor = 0; // 0 = same as color
};

// An object stacked on every block cell (pattern fills, inner shading).
struct Overlay {
    int id = 0;
    bool innerOnly = false;
    float scale = 1.f;
    int zLayer = 0, zOrder = 1;
    int color = 1, detailColor = 0;
};

// An object placed under/over every hazard (glow discs, halos).
struct HazardOverlay {
    int id = 0;
    float scale = 1.f;
    float offsetX = 0.f, offsetY = 0.f; // rotated with `rotation`, scaled with `scale`
    int rotation = 0;
    int zLayer = -1, zOrder = 0;
    int color = 4;
    bool matchRotation = false;
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
    std::map<std::string, Channel> palette;

    std::vector<EdgeLayer> edgeLayers;
    std::vector<Overlay> overlays;
    std::vector<HazardOverlay> hazardOverlays;

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
std::optional<Channel> parseChannel(matjson::Value const& v);
std::string colorToHex(RGB c);

// Writes "id<TAB>frame<TAB>w<TAB>h" for every object ID the game knows, so styles can be
// authored outside the game. Returns the number of rows written.
geode::Result<int> dumpObjectTable(std::filesystem::path const& path);

} // namespace autodeco
