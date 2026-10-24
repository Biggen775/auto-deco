#include "Decorator.hpp"
#include <Geode/utils/file.hpp>
#include <Geode/binding/GJEffectManager.hpp>
#include <Geode/binding/ColorAction.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSpriteColor.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <cctype>
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
static std::vector<Placement> classifyCell(CellSet const& occ, Cell c) {
    auto solid = [&](int dx, int dy) { return occ.count({c.x + dx, c.y + dy}) > 0; };
    unsigned open = 0, diag = 0;
    if (!solid(0, 1)) open |= T;
    if (!solid(1, 0)) open |= R;
    if (!solid(0, -1)) open |= B;
    if (!solid(-1, 0)) open |= L;
    if (!solid(-1, 1)) diag |= TL;
    if (!solid(1, 1)) diag |= TR;
    if (!solid(1, -1)) diag |= BR;
    if (!solid(-1, -1)) diag |= BL;
    return classify(open, diag);
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
    for (auto& [key, rgb] : style.palette) {
        int id = 0;
        if (auto it = named.find(key); it != named.end()) id = it->second;
        else id = std::atoi(key.c_str());
        if (id <= 0) continue;
        auto action = em->getColorAction(id);
        if (!action) continue;
        action->m_fromColor = rgb.cc();
        action->m_toColor = rgb.cc();
        action->m_color = rgb.cc();
        action->m_colorID = id;
    }
    m_layer->updateLevelColors();
}

Result<ApplyReport> Decorator::apply(Style const& style, ApplyOptions const& opts) {
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
    CellSet occ;
    std::vector<GameObject*> blocks, hazards;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (auto obj : CCArrayExt<GameObject*>(m_layer->m_objects)) {
        if (!obj) continue;
        auto p = obj->getPosition();
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        if (solidType(obj)) markRect(occ, m_layer->getObjectRect(obj, false, false));
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
        auto placements = classifyCell(occ, cell);
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
            return true;
        };
        CellSet used;
        for (int x = cx0; x <= cx1; x++) {
            for (int y = cy0; y <= cy1; y++) {
                Cell c{x, y};
                if (occ.count(c)) continue;
                for (auto& rule : style.scatter) {
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

    log::info("Auto Deco: applied '{}' -> {} blocks, {} hazards, {} scatter (group {})",
              style.name, rep.blocksReplaced, rep.hazardsTouched, rep.scatterAdded, rep.group);
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
            if (a) s.palette[key] = RGB{a->m_color.r, a->m_color.g, a->m_color.b};
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
