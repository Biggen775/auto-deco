#include "Decorator.hpp"
#include <Geode/utils/file.hpp>
#include <Geode/binding/GJEffectManager.hpp>
#include <Geode/binding/ColorAction.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSpriteColor.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <cmath>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace autodeco {

// ---------------------------------------------------------------- grid helpers

static constexpr float CELL = 30.f;

struct Cell {
    int x, y;
    bool operator==(Cell const& o) const { return x == o.x && y == o.y; }
};
struct CellHash {
    size_t operator()(Cell const& c) const { return std::hash<long long>()(((long long)c.x << 32) ^ (unsigned)c.y); }
};
using CellSet = std::unordered_set<Cell, CellHash>;

static Cell cellOf(CCPoint p) {
    return {(int)std::floor(p.x / CELL), (int)std::floor(p.y / CELL)};
}
static CCPoint centerOf(Cell c) {
    return {c.x * CELL + CELL / 2, c.y * CELL + CELL / 2};
}

static void markRect(CellSet& set, CCRect const& r) {
    int x0 = (int)std::floor((r.getMinX() + 1.f) / CELL);
    int x1 = (int)std::floor((r.getMaxX() - 1.f) / CELL);
    int y0 = (int)std::floor((r.getMinY() + 1.f) / CELL);
    int y1 = (int)std::floor((r.getMaxY() - 1.f) / CELL);
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) set.insert({x, y});
}

static bool solidType(GameObject* o) {
    return o->m_objectType == GameObjectType::Solid || o->m_objectType == GameObjectType::Slope;
}

static int colorIdOf(GJSpriteColor* c, int def) {
    return c ? c->m_colorID : def;
}

// Classifier input for one cell. GD's y axis points up, so "top" is y + 1.
static void cellMasks(CellSet const& occ, Cell c, unsigned& open, unsigned& diag) {
    auto solid = [&](int dx, int dy) { return occ.count({c.x + dx, c.y + dy}) > 0; };
    open = 0; diag = 0;
    if (!solid(0, 1)) open |= T;
    if (!solid(1, 0)) open |= R;
    if (!solid(0, -1)) open |= B;
    if (!solid(-1, 0)) open |= L;
    if (!solid(-1, 1)) diag |= TL;
    if (!solid(1, 1)) diag |= TR;
    if (!solid(1, -1)) diag |= BR;
    if (!solid(-1, -1)) diag |= BL;
}
static std::vector<Placement> classifyCell(CellSet const& occ, Cell c) {
    unsigned open, diag;
    cellMasks(occ, c, open, diag);
    return classify(open, diag);
}

// Rotate an offset given in the reference orientation by `deg` clockwise (y up).
static CCPoint rotateOffset(float dx, float dy, int deg) {
    switch (normalizeRotation(deg)) {
        case 90:  return {dy, -dx};
        case 180: return {-dx, -dy};
        case 270: return {-dy, dx};
        default:  return {dx, dy};
    }
}

// Builds a fresh object string for a decoration piece.
static ObjString makePiece(int id, CCPoint pos, int rot, float scale, int zLayer, int zOrder, int color, int detail, std::string const& group,
                           float scaleX = 0.f, float scaleY = 0.f) {
    ObjString o;
    o.set("1", std::to_string(id));
    o.set("2", fmt::format("{:.2f}", pos.x));
    o.set("3", fmt::format("{:.2f}", pos.y));
    if (rot) o.set("6", std::to_string(normalizeRotation(rot)));
    float sx = scaleX > 0.f ? scaleX : scale;
    float sy = scaleY > 0.f ? scaleY : scale;
    if (std::fabs(sx - sy) > 0.001f) {
        o.set("32", fmt::format("{:.3f}", std::max(sx, sy)));
        o.set("128", fmt::format("{:.3f}", sx));
        o.set("129", fmt::format("{:.3f}", sy));
    } else if (std::fabs(sx - 1.f) > 0.001f) {
        o.set("32", fmt::format("{:.3f}", sx));
    }
    o.set("24", std::to_string(zLayer));
    o.set("25", std::to_string(zOrder));
    if (color > 0) o.set("21", std::to_string(color));
    if (detail > 0) o.set("22", std::to_string(detail));
    o.set("57", group);
    return o;
}

// Places an EdgeLayer's pieces around one cell given its open-side / open-diagonal masks.
static void placeEdgeLayer(EdgeLayer const& e, CCPoint c, unsigned open, unsigned diag,
                           std::string const& group, std::function<void(ObjString const&)> const& emit) {
    auto put = [&](int id, int rot) {
        if (id <= 0) return;
        rot += e.rotationOffset;
        auto off = rotateOffset(e.offsetX, e.offsetY, rot);
        emit(makePiece(id, {c.x + off.x, c.y + off.y}, rot, e.scale, e.zLayer, e.zOrder, e.color, e.detailColor, group, e.scaleX, e.scaleY));
    };
    if (open & T) put(e.edge, 0);
    if (open & R) put(e.edge, 90);
    if (open & B) put(e.edge, 180);
    if (open & L) put(e.edge, 270);
    if ((open & T) && (open & L)) put(e.corner, 0);
    if ((open & T) && (open & R)) put(e.corner, 90);
    if ((open & R) && (open & B)) put(e.corner, 180);
    if ((open & B) && (open & L)) put(e.corner, 270);
    if ((diag & TL) && !(open & T) && !(open & L)) put(e.innerCorner, 0);
    if ((diag & TR) && !(open & T) && !(open & R)) put(e.innerCorner, 90);
    if ((diag & BR) && !(open & B) && !(open & R)) put(e.innerCorner, 180);
    if ((diag & BL) && !(open & B) && !(open & L)) put(e.innerCorner, 270);
}

// ---------------------------------------------------------------- Customize

Customize Customize::load() {
    auto mod = Mod::get();
    Customize c;
    c.outline = mod->getSavedValue<int>("c-outline", 2);
    c.glow = (float)mod->getSavedValue<double>("c-glow", 1.0);
    c.detail = mod->getSavedValue<bool>("c-detail", true);
    c.hazardGlow = mod->getSavedValue<bool>("c-hazard-glow", true);
    c.useAccent = mod->getSavedValue<bool>("c-use-accent", false);
    auto hex = mod->getSavedValue<std::string>("c-accent", "#ffffff");
    if (auto col = parseColor(matjson::Value(hex))) c.accent = *col;
    c.pulse = mod->getSavedValue<int>("c-pulse", -1);
    c.shader = mod->getSavedValue<int>("c-shader", -1);
    c.spin = mod->getSavedValue<int>("c-spin", -1);
    return c;
}

void Customize::save() const {
    auto mod = Mod::get();
    mod->setSavedValue("c-outline", outline);
    mod->setSavedValue("c-glow", (double)glow);
    mod->setSavedValue("c-detail", detail);
    mod->setSavedValue("c-hazard-glow", hazardGlow);
    mod->setSavedValue("c-use-accent", useAccent);
    mod->setSavedValue("c-accent", colorToHex(accent));
    mod->setSavedValue("c-pulse", pulse);
    mod->setSavedValue("c-shader", shader);
    mod->setSavedValue("c-spin", spin);
}

const char* Customize::pulseName(int v) {
    switch (v) { case 0: return "Off"; case 1: return "Soft"; case 2: return "Strong"; default: return "Style"; }
}
const char* Customize::shaderName(int v) {
    switch (v) { case 0: return "None"; case 1: return "Chromatic"; case 2: return "Vignette"; case 3: return "Bulge"; case 4: return "Pixelate"; default: return "Style"; }
}
const char* Customize::outlineName(int v) {
    switch (v) { case 0: return "Off"; case 1: return "Thin"; case 3: return "Thick"; default: return "Style"; }
}

static bool nameHas(std::string const& name, const char* word) {
    return name.find(word) != std::string::npos;
}

// Applies the Customize panel to a copy of the style.
static Style effectiveStyle(Style const& in, Customize const& c) {
    Style s = in;

    // Outline thickness. Outline layers are 30x2 lines centred (15 - scaleY) from the cell centre.
    std::vector<EdgeLayer> layers;
    for (auto e : s.edgeLayers) {
        if (nameHas(e.name, "outline")) {
            if (c.outline == 0) continue;
            float sy = e.scaleY > 0.f ? e.scaleY : e.scale;
            if (c.outline == 1) sy = 1.f;
            else if (c.outline == 3) sy = std::max(sy * 1.7f, 3.f);
            if (c.outline != 2) {
                e.scaleY = sy;
                if (e.scaleX <= 0.f) e.scaleX = e.scale;
                e.offsetY = 15.f - sy;
            }
        }
        if (nameHas(e.name, "glow") && c.glow <= 0.01f) continue;
        layers.push_back(e);
    }
    s.edgeLayers = layers;

    if (c.glow <= 0.01f || !c.hazardGlow) s.hazardOverlays.clear();
    if (!c.detail) s.overlays.clear();

    for (auto& [key, ch] : s.palette) {
        if (ch.blending) ch.opacity = std::clamp(ch.opacity * c.glow, 0.f, 1.f);
    }
    if (c.useAccent) {
        for (int id : s.effects.accentChannels) {
            auto it = s.palette.find(std::to_string(id));
            if (it != s.palette.end()) it->second.color = c.accent;
        }
    }
    return s;
}

static int pulseLevel(Style const& s, Customize const& c) {
    if (c.pulse >= 0) return c.pulse;
    if (s.effects.pulse == "soft") return 1;
    if (s.effects.pulse == "strong") return 2;
    return 0;
}
static int shaderKind(Style const& s, Customize const& c) {
    if (c.shader >= 0) return c.shader;
    auto& n = s.effects.shader;
    if (n == "chromatic") return 1;
    if (n == "vignette") return 2;
    if (n == "bulge") return 3;
    if (n == "pixelate") return 4;
    return 0;
}
static bool spinOn(Style const& s, Customize const& c) {
    return c.spin >= 0 ? c.spin == 1 : s.effects.spin;
}

static ObjString makeTrigger(int id, CCPoint pos, std::string const& group) {
    ObjString o;
    o.set("1", std::to_string(id));
    o.set("2", fmt::format("{:.2f}", pos.x));
    o.set("3", fmt::format("{:.2f}", pos.y));
    o.set("36", "1");
    o.set("57", group);
    return o;
}

// ---------------------------------------------------------------- Decorator

bool Decorator::isGridBlock(GameObject* obj) const {
    if (obj->m_objectType != GameObjectType::Solid) return false;
    auto rect = m_layer->getObjectRect(obj, false, false);
    if (rect.size.width < 27.f || rect.size.width > 33.f) return false;
    if (rect.size.height < 27.f || rect.size.height > 33.f) return false;
    auto p = obj->getPosition();
    auto offX = std::fmod(std::fabs(p.x - CELL / 2), CELL);
    auto offY = std::fmod(std::fabs(p.y - CELL / 2), CELL);
    auto near0 = [](float v) { return v < 0.5f || v > CELL - 0.5f; };
    return near0(offX) && near0(offY);
}

std::filesystem::path Decorator::backupPath() const {
    auto dir = Mod::get()->getSaveDir() / "backups";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string key;
    auto level = m_layer->m_level;
    if (level && level->m_levelID.value() > 0) key = fmt::format("id-{}", level->m_levelID.value());
    else {
        std::string name = level ? std::string(level->m_levelName) : "unknown";
        for (auto& ch : name) if (!std::isalnum((unsigned char)ch)) ch = '_';
        key = "name-" + name;
    }
    return dir / (key + ".json");
}

bool Decorator::hasBackup() const {
    std::error_code ec;
    return std::filesystem::exists(backupPath(), ec);
}

void Decorator::applyPalette(Style const& style) {
    auto settings = m_layer->m_levelSettings;
    if (!settings || !settings->m_effectManager) return;
    auto em = settings->m_effectManager;
    static const std::unordered_map<std::string, int> named = {
        {"bg", 1000}, {"ground", 1001}, {"line", 1002}, {"3dl", 1003},
        {"object", 1004}, {"ground2", 1009},
    };
    for (auto& [key, ch] : style.palette) {
        int id = 0;
        if (auto it = named.find(key); it != named.end()) id = it->second;
        else id = std::atoi(key.c_str());
        if (id <= 0) continue;
        auto action = em->getColorAction(id);
        if (!action) continue;
        action->m_fromColor = ch.color.cc();
        action->m_toColor = ch.color.cc();
        action->m_color = ch.color.cc();
        action->m_colorID = id;
        action->m_blending = ch.blending;
        action->m_fromOpacity = ch.opacity;
        action->m_toOpacity = ch.opacity;
        action->m_currentOpacity = ch.opacity;
    }
    m_layer->updateLevelColors();
}

Result<ApplyReport> Decorator::apply(Style const& styleIn, ApplyOptions const& opts) {
    Style style = effectiveStyle(styleIn, opts.custom);
    if (!m_layer || !m_layer->m_objects) return Err("no editor layer");
    if (!style.hasBlocks()) return Err("style has no block pieces");

    // Re-applying: restore the original layout first so styles don't stack.
    if (hasBackup()) {
        auto r = undo();
        if (!r) return Err(r.unwrapErr());
    }

    if (auto ui = EditorUI::get()) ui->deselectAll();

    ApplyReport rep;
    gd::unordered_set<int> exclude;
    rep.group = m_layer->getNextFreeGroupID(exclude);
    if (rep.group <= 0) return Err("no free group id");

    // 1. Occupancy from every solid in the level (not just the selection), so edge
    //    classification is right at the border of a selection.
    CellSet occ, avoid;
    std::vector<GameObject*> blocks, hazards;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (auto obj : CCArrayExt<GameObject*>(m_layer->m_objects)) {
        if (!obj) continue;
        auto p = obj->getPosition();
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        if (solidType(obj)) markRect(occ, m_layer->getObjectRect(obj, false, false));
        else if (obj->m_objectType != GameObjectType::Decoration) {
            // Orbs, pads, portals, hazards, triggers: scatter must never sit on top of these.
            markRect(avoid, m_layer->getObjectRect(obj, false, false));
        }
        bool eligible = !opts.selectionOnly || obj->m_isSelected;
        if (!eligible) continue;
        if (isGridBlock(obj)) blocks.push_back(obj);
        else if (opts.hazards && obj->m_objectType == GameObjectType::Hazard) hazards.push_back(obj);
    }
    if (blocks.empty() && hazards.empty()) return Err(opts.selectionOnly ? "nothing selected to decorate" : "no blocks found");

    // 2. Build replacement strings, remember originals.
    std::vector<std::string> backup;
    std::string created;
    auto groupStr = std::to_string(rep.group);
    auto appendGroup = [&](ObjString& o) {
        auto g = o.get("57");
        o.set("57", g.empty() ? groupStr : g + "." + groupStr);
    };
    auto emit = [&](ObjString const& o) {
        created += o.str();
        created += ';';
    };

    for (auto obj : blocks) {
        auto orig = std::string(obj->getSaveString(m_layer));
        auto cell = cellOf(obj->getPosition());
        unsigned open, diag;
        cellMasks(occ, cell, open, diag);
        auto placements = classify(open, diag);
        auto center = centerOf(cell);

        // Decoration layers around / on top of the block.
        for (auto& e : style.edgeLayers) placeEdgeLayer(e, center, open, diag, groupStr, emit);
        for (auto& ov : style.overlays) {
            if (ov.innerOnly && open) continue;
            emit(makePiece(ov.id, center, 0, ov.scale, ov.zLayer, ov.zOrder,
                           opts.recolor ? ov.color : 0, opts.recolor ? ov.detailColor : 0, groupStr));
        }

        bool first = true;
        for (auto& pl : placements) {
            auto piece = style.pieceFor(pl.role);
            if (piece.id <= 0) continue;
            auto o = ObjString::parse(orig);
            o.set("1", std::to_string(piece.id));
            o.set("6", std::to_string(normalizeRotation(pl.rotation + piece.rotationOffset)));
            o.erase("4"); o.erase("5");      // flips
            o.erase("32"); o.erase("128"); o.erase("129"); // scale
            if (opts.recolor) {
                o.set("21", std::to_string(style.blockBaseChannel));
                o.set("22", std::to_string(style.blockDetailChannel));
                o.erase("41"); o.erase("43"); // HSV
                o.erase("42"); o.erase("44");
            }
            if (!first) o.set("25", std::to_string(std::atoi(o.get("25", "0").c_str()) + 1));
            appendGroup(o);
            emit(o);
            first = false;
        }
        backup.push_back(orig);
        rep.blocksReplaced++;
    }

    for (auto obj : hazards) {
        for (auto& h : style.hazardOverlays) {
            int rot = h.rotation + (h.matchRotation ? (int)std::lround(obj->getRotation()) : 0);
            float sc = h.scale * std::max(obj->m_scaleX, 0.1f);
            auto off = rotateOffset(h.offsetX * sc, h.offsetY * sc, rot);
            auto p = obj->getPosition();
            emit(makePiece(h.id, {p.x + off.x, p.y + off.y}, rot, sc, h.zLayer, h.zOrder, h.color, 0, groupStr));
        }
        auto remap = style.hazards.find(obj->m_objectID);
        bool doRemap = remap != style.hazards.end();
        bool doRecolor = opts.recolor && style.recolorHazards;
        if (!doRemap && !doRecolor) continue;
        auto orig = std::string(obj->getSaveString(m_layer));
        auto o = ObjString::parse(orig);
        if (doRemap) o.set("1", std::to_string(remap->second));
        if (doRecolor) {
            o.set("21", std::to_string(style.hazardBaseChannel));
            o.set("22", std::to_string(style.hazardDetailChannel));
            o.erase("41"); o.erase("43"); o.erase("42"); o.erase("44");
        }
        appendGroup(o);
        emit(o);
        backup.push_back(orig);
        rep.hazardsTouched++;
    }

    // 3. Scatter decoration in empty cells of the level's bounding box.
    float mult = Mod::get()->getSettingValue<double>("scatter-multiplier");
    if (opts.scatter && mult > 0.f && !style.scatter.empty() && maxX > minX) {
        uint32_t seed = (uint32_t)(m_layer->m_level ? m_layer->m_level->m_levelID.value() : 0);
        if (seed == 0) seed = (uint32_t)std::hash<std::string>()(m_layer->m_level ? std::string(m_layer->m_level->m_levelName) : "");
        std::mt19937 rng(seed ^ 0xA0D3C0);
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        int cx0 = (int)std::floor(minX / CELL) - 2, cx1 = (int)std::floor(maxX / CELL) + 2;
        int cy0 = std::max(0, (int)std::floor(minY / CELL) - 2), cy1 = (int)std::floor(maxY / CELL) + 6;
        if (opts.selectionOnly) {
            // Limit scatter to the bounding box of the selection.
            float sx0 = 1e9f, sx1 = -1e9f, sy0 = 1e9f, sy1 = -1e9f;
            for (auto obj : blocks) { auto p = obj->getPosition(); sx0 = std::min(sx0, p.x); sx1 = std::max(sx1, p.x); sy0 = std::min(sy0, p.y); sy1 = std::max(sy1, p.y); }
            for (auto obj : hazards) { auto p = obj->getPosition(); sx0 = std::min(sx0, p.x); sx1 = std::max(sx1, p.x); sy0 = std::min(sy0, p.y); sy1 = std::max(sy1, p.y); }
            cx0 = (int)std::floor(sx0 / CELL) - 1; cx1 = (int)std::floor(sx1 / CELL) + 1;
            cy0 = std::max(0, (int)std::floor(sy0 / CELL) - 1); cy1 = (int)std::floor(sy1 / CELL) + 1;
        }
        auto clear = [&](Cell c, int radius) {
            for (int dx = -radius; dx <= radius; dx++)
                for (int dy = -radius; dy <= radius; dy++)
                    if (occ.count({c.x + dx, c.y + dy})) return false;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    if (avoid.count({c.x + dx, c.y + dy})) return false;
            return true;
        };
        // Optional spinning deco (1.9 rotating lines / balls, anchored at their base).
        std::vector<ScatterRule> rules = style.scatter;
        if (spinOn(style, opts.custom)) {
            for (int id : {1521, 1523, 376, 378}) {
                ScatterRule r; r.id = id; r.density = 0.004f; r.scaleMin = 0.6f; r.scaleMax = 1.1f;
                r.randomRotation = true; r.zLayer = -5; r.color = style.effects.spinColor; r.minClearance = 2;
                rules.push_back(r);
            }
        }
        CellSet used;
        for (int x = cx0; x <= cx1; x++) {
            for (int y = cy0; y <= cy1; y++) {
                Cell c{x, y};
                if (occ.count(c) || avoid.count(c)) continue;
                for (auto& rule : rules) {
                    if (unit(rng) >= rule.density * mult) continue;
                    if (used.count(c) || !clear(c, rule.minClearance)) break;
                    used.insert(c);
                    auto pos = centerOf(c);
                    pos.x += (unit(rng) - 0.5f) * CELL * 0.6f;
                    pos.y += (unit(rng) - 0.5f) * CELL * 0.6f;
                    float scale = rule.scaleMin + (rule.scaleMax - rule.scaleMin) * unit(rng);
                    ObjString o;
                    o.set("1", std::to_string(rule.id));
                    o.set("2", fmt::format("{:.2f}", pos.x));
                    o.set("3", fmt::format("{:.2f}", pos.y));
                    if (rule.randomRotation) o.set("6", std::to_string((int)(unit(rng) * 360.f)));
                    if (std::fabs(scale - 1.f) > 0.001f) o.set("32", fmt::format("{:.3f}", scale));
                    o.set("24", std::to_string(rule.zLayer));
                    o.set("25", std::to_string(rule.zOrder));
                    o.set("21", std::to_string(rule.color));
                    o.set("57", groupStr);
                    emit(o);
                    rep.scatterAdded++;
                    break;
                }
            }
        }
    }

    // 3b. Effects: pulse triggers along the level and a shader trigger at the start.
    if (opts.recolor && maxX > minX) {
        float trigY = std::max(maxY, minY) + 150.f;
        float startX = std::max(minX, 15.f);
        int pulse = pulseLevel(style, opts.custom);
        if (pulse > 0) {
            float spacing = pulse == 2 ? 120.f : 240.f;
            float fadeOut = pulse == 2 ? 0.30f : 0.45f;
            for (float x = startX; x <= maxX; x += spacing) {
                for (int chId : style.effects.pulseChannels) {
                    auto it = style.palette.find(std::to_string(chId));
                    if (it == style.palette.end()) continue;
                    auto c = it->second.color;
                    float k = pulse == 2 ? 0.85f : 0.6f;
                    auto up = [&](uint8_t v) { return (int)std::lround(v + (255 - v) * k); };
                    auto o = makeTrigger(1006, {x, trigY}, groupStr);
                    o.set("51", std::to_string(chId));
                    o.set("52", "0");
                    o.set("48", "0");
                    o.set("7", std::to_string(up(c.r)));
                    o.set("8", std::to_string(up(c.g)));
                    o.set("9", std::to_string(up(c.b)));
                    o.set("45", "0.06");
                    o.set("46", "0.04");
                    o.set("47", fmt::format("{:.2f}", fadeOut));
                    emit(o);
                    rep.triggersAdded++;
                }
            }
        }
        int shader = shaderKind(style, opts.custom);
        if (shader > 0) {
            CCPoint sp{startX, trigY};
            ObjString o;
            switch (shader) {
                case 1: // chromatic aberration
                    o = makeTrigger(2910, sp, groupStr);
                    o.set("10", "0.5"); o.set("180", "3"); o.set("189", "1"); o.set("188", "1"); o.set("190", "1");
                    break;
                case 2: // lens circle vignette around the player
                    o = makeTrigger(2913, sp, groupStr);
                    o.set("10", "1"); o.set("179", "220"); o.set("181", "140"); o.set("176", "0.7"); o.set("138", "1");
                    break;
                case 3: // bulge following the player
                    o = makeTrigger(2916, sp, groupStr);
                    o.set("10", "1"); o.set("176", "0.12"); o.set("180", "260"); o.set("188", "1"); o.set("138", "1");
                    break;
                default: // pixelate
                    o = makeTrigger(2912, sp, groupStr);
                    o.set("10", "0.5"); o.set("180", "3"); o.set("189", "3"); o.set("188", "1"); o.set("190", "1"); o.set("194", "1"); o.set("515", "1");
                    break;
            }
            emit(o);
            rep.triggersAdded++;
        }
    }

    // 4. Commit: remove originals, add the new objects, recolor.
    for (auto obj : blocks) m_layer->removeObject(obj, true);
    for (auto obj : hazards) {
        bool touched = style.hazards.count(obj->m_objectID) || (opts.recolor && style.recolorHazards);
        if (touched) m_layer->removeObject(obj, true);
    }
    if (!created.empty()) m_layer->createObjectsFromString(created, true, true);
    if (opts.recolor) applyPalette(style);

    // 5. Backup file so Undo can restore the layout later (even after a restart).
    auto json = matjson::Value::object();
    json.set("group", rep.group);
    json.set("style", style.id);
    auto arr = matjson::Value::array();
    for (auto& s : backup) arr.push(s);
    json.set("objects", arr);
    auto w = file::writeString(backupPath(), json.dump(0));
    if (!w) log::warn("Auto Deco: could not write backup: {}", w.unwrapErr());

    log::info("Auto Deco: applied '{}' -> {} blocks, {} hazards, {} scatter, {} triggers (group {})",
              style.name, rep.blocksReplaced, rep.hazardsTouched, rep.scatterAdded, rep.triggersAdded, rep.group);
    return Ok(rep);
}

Result<bool> Decorator::undo() {
    if (!hasBackup()) return Ok(false);
    auto json = file::readJson(backupPath());
    if (!json) return Err(fmt::format("backup unreadable: {}", json.unwrapErr()));
    auto v = json.unwrap();
    int group = (int)v["group"].asInt().unwrapOr(0);
    if (group <= 0) return Err("backup has no group");

    if (auto ui = EditorUI::get()) ui->deselectAll();

    // Remove every object carrying our group.
    std::vector<GameObject*> doomed;
    for (auto obj : CCArrayExt<GameObject*>(m_layer->m_objects)) {
        if (!obj || !obj->m_groups) continue;
        for (int i = 0; i < obj->m_groupCount && i < 10; i++) {
            if ((*obj->m_groups)[i] == group) { doomed.push_back(obj); break; }
        }
    }
    for (auto obj : doomed) m_layer->removeObject(obj, true);

    std::string restore;
    if (v.contains("objects") && v["objects"].isArray()) {
        for (auto& s : v["objects"].asArray().unwrap()) {
            restore += s.asString().unwrapOr("");
            restore += ';';
        }
    }
    if (!restore.empty()) m_layer->createObjectsFromString(restore, true, true);

    std::error_code ec;
    std::filesystem::remove(backupPath(), ec);
    log::info("Auto Deco: undo removed {} objects, restored {}", doomed.size(), v["objects"].size());
    return Ok(true);
}

// ---------------------------------------------------------------- learning

Result<Style> Decorator::learnFromSelection(std::string const& name) {
    auto ui = EditorUI::get();
    if (!ui) return Err("no editor ui");
    auto selected = ui->getSelectedObjects();
    if (!selected || selected->count() == 0) return Err("select a sample first");

    Style s;
    s.name = name;
    s.id = name;
    for (auto& ch : s.id) ch = std::isalnum((unsigned char)ch) ? (char)std::tolower((unsigned char)ch) : '-';

    // Occupancy from the selection only: the sample is meant to be an isolated strip.
    CellSet occ;
    std::vector<GameObject*> blocks;
    int blockBase = -1, blockDetail = -1, hazBase = -1, hazDetail = -1;
    for (auto obj : CCArrayExt<GameObject*>(selected)) {
        if (!obj) continue;
        if (solidType(obj)) markRect(occ, m_layer->getObjectRect(obj, false, false));
        if (isGridBlock(obj)) {
            blocks.push_back(obj);
            if (blockBase < 0) { blockBase = colorIdOf(obj->m_baseColor, 1); blockDetail = colorIdOf(obj->m_detailColor, 2); }
        } else if (obj->m_objectType == GameObjectType::Hazard) {
            if (hazBase < 0) { hazBase = colorIdOf(obj->m_baseColor, 1); hazDetail = colorIdOf(obj->m_detailColor, 2); }
        } else if (obj->m_objectType == GameObjectType::Decoration) {
            bool found = false;
            for (auto& r : s.scatter) if (r.id == obj->m_objectID) { found = true; break; }
            if (!found) {
                ScatterRule r;
                r.id = obj->m_objectID;
                r.density = 0.01f;
                r.scaleMin = r.scaleMax = obj->getScale();
                r.zLayer = (int)obj->m_zLayer;
                r.zOrder = obj->m_zOrder;
                r.color = colorIdOf(obj->m_baseColor, 3);
                s.scatter.push_back(r);
            }
        }
    }
    if (blocks.empty()) return Err("selection has no 1x1 blocks");

    // Vote per role on (object id, rotation offset).
    std::array<std::map<std::pair<int, int>, int>, (size_t)Role::COUNT> votes;
    for (auto obj : blocks) {
        auto placements = classifyCell(occ, cellOf(obj->getPosition()));
        if (placements.empty()) continue;
        auto base = placements[0];
        int rot = normalizeRotation((int)std::lround(obj->getRotation()));
        int offset = normalizeRotation(rot - base.rotation);
        votes[(size_t)base.role][{obj->m_objectID, offset}]++;
    }
    for (size_t i = 0; i < (size_t)Role::COUNT; i++) {
        if (votes[i].empty()) continue;
        auto best = std::max_element(votes[i].begin(), votes[i].end(),
                                     [](auto& a, auto& b) { return a.second < b.second; });
        s.blocks[i] = Piece{best->first.first, best->first.second};
    }
    if (!s.hasBlocks()) return Err("sample needs at least an inner block and an edge block");

    s.blockBaseChannel = blockBase > 0 ? blockBase : 1;
    s.blockDetailChannel = blockDetail > 0 ? blockDetail : 2;
    s.hazardBaseChannel = hazBase > 0 ? hazBase : s.blockBaseChannel;
    s.hazardDetailChannel = hazDetail > 0 ? hazDetail : s.blockDetailChannel;

    // Palette: level colours + every channel the sample uses.
    auto em = m_layer->m_levelSettings ? m_layer->m_levelSettings->m_effectManager : nullptr;
    if (em) {
        auto grab = [&](std::string const& key, int id) {
            auto a = em->getColorAction(id);
            if (!a) return;
            Channel ch;
            ch.color = RGB{a->m_color.r, a->m_color.g, a->m_color.b};
            ch.blending = a->m_blending;
            ch.opacity = a->m_currentOpacity > 0.f ? a->m_currentOpacity : 1.f;
            s.palette[key] = ch;
        };
        grab("bg", 1000); grab("ground", 1001); grab("line", 1002); grab("object", 1004); grab("ground2", 1009);
        std::set<int> chans = {s.blockBaseChannel, s.blockDetailChannel, s.hazardBaseChannel, s.hazardDetailChannel};
        for (auto& r : s.scatter) chans.insert(r.color);
        for (int c : chans) if (c > 0 && c < 1000) grab(std::to_string(c), c);
    }
    s.description = fmt::format("Learned from a {}-block sample", blocks.size());
    return Ok(s);
}

} // namespace autodeco
