#pragma once
#include "Style.hpp"
#include "ObjString.hpp"
#include <Geode/Geode.hpp>

namespace autodeco {

struct ApplyOptions {
    bool selectionOnly = false;
    bool scatter = true;
    bool recolor = true;
    bool hazards = true;
};

struct ApplyReport {
    int blocksReplaced = 0;
    int hazardsTouched = 0;
    int scatterAdded = 0;
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

    std::filesystem::path backupPath() const;
    bool isGridBlock(GameObject* obj) const;
    void applyPalette(Style const& style);
};

} // namespace autodeco
