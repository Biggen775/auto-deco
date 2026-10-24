#pragma once
#include "Style.hpp"
#include "ObjString.hpp"
#include <Geode/Geode.hpp>
#include <set>

namespace autodeco {

// User customisation on top of a style. Persisted in the mod's saved values.
struct Customize {
    int outline = 2;          // 0 off, 1 thin, 2 as style, 3 thick
    float glow = 1.f;         // multiplier on blended channel opacity, 0 = no glow
    bool detail = true;       // per-cell overlays
    bool hazardGlow = true;   // glow discs behind hazards
    bool useAccent = false;   // replace the style's accent channels with `accent`
    RGB accent{255, 255, 255};
    int pulse = -1;           // -1 style default, 0 off, 1 soft, 2 strong
    int shader = -1;          // -1 style default, 0 none, 1 chromatic, 2 vignette, 3 bulge, 4 pixelate
    int spin = -1;            // -1 style default, 0 off, 1 on

    static Customize load();
    void save() const;
    static const char* pulseName(int v);
    static const char* shaderName(int v);
    static const char* outlineName(int v);
};

struct ApplyOptions {
    bool selectionOnly = false;
    bool scatter = true;
    bool recolor = true;
    bool hazards = true;
    Customize custom;
};

struct ApplyReport {
    int blocksReplaced = 0;
    int hazardsTouched = 0;
    int scatterAdded = 0;
    int triggersAdded = 0;
    int group = 0;
};

class Decorator {
public:
    explicit Decorator(LevelEditorLayer* layer) : m_layer(layer) {}

    geode::Result<ApplyReport> apply(Style const& style, ApplyOptions const& opts);
    // Removes everything the mod added and restores the original objects. Returns false if
    // there was nothing to undo.
    geode::Result<bool> undo();
    bool hasBackup() const;

    // Builds a Style from the currently selected objects (a hand-made sample strip).
    geode::Result<Style> learnFromSelection(std::string const& name);

private:
    LevelEditorLayer* m_layer;
    std::set<int> m_knownGroups; // groups seen in the backup during undo(), carried into the next apply()

    std::filesystem::path backupPath() const;
    bool isGridBlock(GameObject* obj) const;
    void applyPalette(Style const& style);
};

} // namespace autodeco
