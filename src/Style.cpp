#include "Style.hpp"
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

namespace autodeco {

// ---------- helpers ----------

std::optional<RGB> parseColor(matjson::Value const& v) {
    if (v.isString()) {
        auto s = v.asString().unwrapOr("");
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        if (s.size() != 6) return std::nullopt;
        auto hex = [&](int i) -> int {
            return (int)std::strtol(s.substr(i, 2).c_str(), nullptr, 16);
        };
        return RGB{(uint8_t)hex(0), (uint8_t)hex(2), (uint8_t)hex(4)};
    }
    if (v.isArray()) {
        auto arr = v.asArray().unwrap();
        if (arr.size() < 3) return std::nullopt;
        return RGB{
            (uint8_t)std::clamp<intmax_t>(arr[0].asInt().unwrapOr(0), 0, 255),
            (uint8_t)std::clamp<intmax_t>(arr[1].asInt().unwrapOr(0), 0, 255),
            (uint8_t)std::clamp<intmax_t>(arr[2].asInt().unwrapOr(0), 0, 255),
        };
    }
    return std::nullopt;
}

std::optional<Channel> parseChannel(matjson::Value const& v) {
    Channel ch;
    if (v.isObject()) {
        if (!v.contains("color")) return std::nullopt;
        auto c = parseColor(v["color"]);
        if (!c) return std::nullopt;
        ch.color = *c;
        if (v.contains("blending")) ch.blending = v["blending"].asBool().unwrapOr(false);
        if (v.contains("opacity")) ch.opacity = (float)v["opacity"].asDouble().unwrapOr(1.0);
        return ch;
    }
    auto c = parseColor(v);
    if (!c) return std::nullopt;
    ch.color = *c;
    return ch;
}

std::string colorToHex(RGB c) {
    return fmt::format("#{:02x}{:02x}{:02x}", c.r, c.g, c.b);
}

static int getInt(matjson::Value const& v, std::string_view key, int def) {
    if (!v.contains(key)) return def;
    auto r = v[key].asInt();
    return r.isOk() ? (int)r.unwrap() : def;
}
static float getFloat(matjson::Value const& v, std::string_view key, float def) {
    if (!v.contains(key)) return def;
    auto r = v[key].asDouble();
    return r.isOk() ? (float)r.unwrap() : def;
}
static bool getBool(matjson::Value const& v, std::string_view key, bool def) {
    if (!v.contains(key)) return def;
    auto r = v[key].asBool();
    return r.isOk() ? r.unwrap() : def;
}
static std::string getStr(matjson::Value const& v, std::string_view key, std::string const& def) {
    if (!v.contains(key)) return def;
    auto r = v[key].asString();
    return r.isOk() ? r.unwrap() : def;
}

// ---------- Style ----------

Piece Style::pieceFor(Role role) const {
    auto has = [&](Role r) { return blocks[(size_t)r].has_value(); };
    auto get = [&](Role r) { return *blocks[(size_t)r]; };
    if (has(role)) return get(role);
    switch (role) {
        case Role::InnerCorner: return has(Role::Inner) ? get(Role::Inner) : Piece{};
        case Role::Single:      return has(Role::Cap) ? get(Role::Cap) : has(Role::Edge) ? get(Role::Edge) : Piece{};
        case Role::Cap:         return has(Role::Corner) ? get(Role::Corner) : has(Role::Edge) ? get(Role::Edge) : Piece{};
        case Role::Pillar:      return has(Role::Edge) ? get(Role::Edge) : Piece{};
        case Role::Corner:      return has(Role::Edge) ? get(Role::Edge) : Piece{};
        case Role::Edge:        return has(Role::Inner) ? get(Role::Inner) : Piece{};
        default:                return Piece{};
    }
}

Result<Style> Style::fromJson(matjson::Value const& v, std::string const& id) {
    if (!v.isObject()) return Err("style is not a JSON object");
    Style s;
    s.id = id;
    s.name = getStr(v, "name", id);
    s.description = getStr(v, "description", "");

    if (v.contains("palette") && v["palette"].isObject()) {
        for (auto& val : v["palette"]) {
            auto key = val.getKey();
            if (!key) continue;
            if (auto c = parseChannel(val)) s.palette[*key] = *c;
        }
    }

    if (v.contains("edgeLayers") && v["edgeLayers"].isArray()) {
        for (auto& ev : v["edgeLayers"].asArray().unwrap()) {
            if (!ev.isObject()) continue;
            EdgeLayer e;
            e.name = getStr(ev, "name", "");
            e.edge = getInt(ev, "edge", 0);
            e.corner = getInt(ev, "corner", 0);
            e.innerCorner = getInt(ev, "innerCorner", 0);
            if (!e.edge && !e.corner && !e.innerCorner) continue;
            e.rotationOffset = getInt(ev, "rotation", 0);
            e.offsetX = getFloat(ev, "offsetX", 0.f);
            e.offsetY = getFloat(ev, "offsetY", 0.f);
            e.scale = getFloat(ev, "scale", 1.f);
            e.scaleX = getFloat(ev, "scaleX", 0.f);
            e.scaleY = getFloat(ev, "scaleY", 0.f);
            e.zLayer = getInt(ev, "zLayer", 3);
            e.zOrder = getInt(ev, "zOrder", 0);
            e.color = getInt(ev, "color", 2);
            e.detailColor = getInt(ev, "detailColor", 0);
            s.edgeLayers.push_back(e);
        }
    }

    if (v.contains("overlays") && v["overlays"].isArray()) {
        for (auto& ov : v["overlays"].asArray().unwrap()) {
            if (!ov.isObject()) continue;
            Overlay o;
            o.id = getInt(ov, "id", 0);
            if (o.id <= 0) continue;
            o.innerOnly = getBool(ov, "innerOnly", false);
            o.scale = getFloat(ov, "scale", 1.f);
            o.zLayer = getInt(ov, "zLayer", 0);
            o.zOrder = getInt(ov, "zOrder", 1);
            o.color = getInt(ov, "color", 1);
            o.detailColor = getInt(ov, "detailColor", 0);
            s.overlays.push_back(o);
        }
    }

    if (v.contains("hazardOverlays") && v["hazardOverlays"].isArray()) {
        for (auto& hv : v["hazardOverlays"].asArray().unwrap()) {
            if (!hv.isObject()) continue;
            HazardOverlay h;
            h.id = getInt(hv, "id", 0);
            if (h.id <= 0) continue;
            h.scale = getFloat(hv, "scale", 1.f);
            h.offsetX = getFloat(hv, "offsetX", 0.f);
            h.offsetY = getFloat(hv, "offsetY", 0.f);
            h.rotation = getInt(hv, "rotation", 0);
            h.zLayer = getInt(hv, "zLayer", -1);
            h.zOrder = getInt(hv, "zOrder", 0);
            h.color = getInt(hv, "color", 4);
            h.matchRotation = getBool(hv, "matchRotation", false);
            s.hazardOverlays.push_back(h);
        }
    }

    if (v.contains("blocks") && v["blocks"].isObject()) {
        auto& b = v["blocks"];
        for (size_t i = 0; i < (size_t)Role::COUNT; i++) {
            auto key = roleName((Role)i);
            if (!b.contains(key)) continue;
            auto& pv = b[key];
            Piece p;
            if (pv.isNumber()) {
                p.id = (int)pv.asInt().unwrapOr(0);
            } else if (pv.isObject()) {
                p.id = getInt(pv, "id", 0);
                p.rotationOffset = getInt(pv, "rotation", 0);
            }
            if (p.id > 0) s.blocks[i] = p;
        }
        s.blockBaseChannel = getInt(b, "baseColor", 1);
        s.blockDetailChannel = getInt(b, "detailColor", 2);
    }

    if (v.contains("hazards") && v["hazards"].isObject()) {
        auto& h = v["hazards"];
        s.recolorHazards = getBool(h, "recolor", true);
        s.hazardBaseChannel = getInt(h, "baseColor", 1);
        s.hazardDetailChannel = getInt(h, "detailColor", 2);
        if (h.contains("remap") && h["remap"].isObject()) {
            for (auto& val : h["remap"]) {
                auto key = val.getKey();
                if (!key) continue;
                int from = std::atoi(key->c_str());
                int to = (int)val.asInt().unwrapOr(0);
                if (from > 0 && to > 0) s.hazards[from] = to;
            }
        }
    }

    if (v.contains("scatter") && v["scatter"].isArray()) {
        for (auto& sv : v["scatter"].asArray().unwrap()) {
            if (!sv.isObject()) continue;
            ScatterRule r;
            r.id = getInt(sv, "id", 0);
            if (r.id <= 0) continue;
            r.density = getFloat(sv, "density", 0.01f);
            if (sv.contains("scale") && sv["scale"].isArray()) {
                auto arr = sv["scale"].asArray().unwrap();
                if (arr.size() >= 2) {
                    r.scaleMin = (float)arr[0].asDouble().unwrapOr(1.0);
                    r.scaleMax = (float)arr[1].asDouble().unwrapOr(1.0);
                }
            } else {
                r.scaleMin = r.scaleMax = getFloat(sv, "scale", 1.f);
            }
            r.randomRotation = getBool(sv, "randomRotation", false);
            r.zLayer = getInt(sv, "zLayer", -3);
            r.zOrder = getInt(sv, "zOrder", 0);
            r.color = getInt(sv, "color", 3);
            r.minClearance = getInt(sv, "clearance", 0);
            s.scatter.push_back(r);
        }
    }
    return Ok(s);
}

matjson::Value Style::toJson() const {
    auto v = matjson::Value::object();
    v.set("name", name);
    v.set("description", description);

    auto pal = matjson::Value::object();
    for (auto& [k, ch] : palette) {
        if (!ch.blending && ch.opacity >= 0.999f) { pal.set(k, colorToHex(ch.color)); continue; }
        auto o = matjson::Value::object();
        o.set("color", colorToHex(ch.color));
        o.set("blending", ch.blending);
        o.set("opacity", (double)ch.opacity);
        pal.set(k, o);
    }
    v.set("palette", pal);

    auto el = matjson::Value::array();
    for (auto& e : edgeLayers) {
        auto o = matjson::Value::object();
        o.set("name", e.name);
        o.set("edge", e.edge); o.set("corner", e.corner); o.set("innerCorner", e.innerCorner);
        o.set("rotation", e.rotationOffset);
        o.set("offsetX", (double)e.offsetX); o.set("offsetY", (double)e.offsetY);
        o.set("scale", (double)e.scale);
        if (e.scaleX > 0.f) o.set("scaleX", (double)e.scaleX);
        if (e.scaleY > 0.f) o.set("scaleY", (double)e.scaleY);
        o.set("zLayer", e.zLayer); o.set("zOrder", e.zOrder);
        o.set("color", e.color); o.set("detailColor", e.detailColor);
        el.push(o);
    }
    v.set("edgeLayers", el);

    auto ol = matjson::Value::array();
    for (auto& ov : overlays) {
        auto o = matjson::Value::object();
        o.set("id", ov.id); o.set("innerOnly", ov.innerOnly); o.set("scale", (double)ov.scale);
        o.set("zLayer", ov.zLayer); o.set("zOrder", ov.zOrder);
        o.set("color", ov.color); o.set("detailColor", ov.detailColor);
        ol.push(o);
    }
    v.set("overlays", ol);

    auto hl = matjson::Value::array();
    for (auto& h : hazardOverlays) {
        auto o = matjson::Value::object();
        o.set("id", h.id); o.set("scale", (double)h.scale);
        o.set("zLayer", h.zLayer); o.set("zOrder", h.zOrder);
        o.set("offsetX", (double)h.offsetX); o.set("offsetY", (double)h.offsetY);
        o.set("rotation", h.rotation);
        o.set("color", h.color); o.set("matchRotation", h.matchRotation);
        hl.push(o);
    }
    v.set("hazardOverlays", hl);

    auto b = matjson::Value::object();
    for (size_t i = 0; i < (size_t)Role::COUNT; i++) {
        if (!blocks[i]) continue;
        auto p = matjson::Value::object();
        p.set("id", blocks[i]->id);
        p.set("rotation", blocks[i]->rotationOffset);
        b.set(roleName((Role)i), p);
    }
    b.set("baseColor", blockBaseChannel);
    b.set("detailColor", blockDetailChannel);
    v.set("blocks", b);

    auto h = matjson::Value::object();
    h.set("recolor", recolorHazards);
    h.set("baseColor", hazardBaseChannel);
    h.set("detailColor", hazardDetailChannel);
    auto remap = matjson::Value::object();
    for (auto& [from, to] : hazards) remap.set(std::to_string(from), to);
    h.set("remap", remap);
    v.set("hazards", h);

    auto sc = matjson::Value::array();
    for (auto& r : scatter) {
        auto o = matjson::Value::object();
        o.set("id", r.id);
        o.set("density", (double)r.density);
        o.set("scale", matjson::Value(std::vector<matjson::Value>{(double)r.scaleMin, (double)r.scaleMax}));
        o.set("randomRotation", r.randomRotation);
        o.set("zLayer", r.zLayer);
        o.set("zOrder", r.zOrder);
        o.set("color", r.color);
        o.set("clearance", r.minClearance);
        sc.push(o);
    }
    v.set("scatter", sc);
    return v;
}

// ---------- object table dump ----------

Result<int> dumpObjectTable(std::filesystem::path const& path) {
    auto toolbox = ObjectToolbox::sharedState();
    if (!toolbox) return Err("no object toolbox");
    auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    std::string out = "id\tframe\tw\th\n";
    int rows = 0;
    for (int id = 1; id <= 4500; id++) {
        const char* frame = toolbox->intKeyToFrame(id);
        if (!frame || !*frame) continue;
        float w = 0, h = 0;
        if (auto sf = cache->spriteFrameByName(frame)) {
            auto sz = sf->getOriginalSize();
            w = sz.width; h = sz.height;
        }
        out += fmt::format("{}\t{}\t{:.0f}\t{:.0f}\n", id, frame, w, h);
        rows++;
    }
    GEODE_UNWRAP(file::writeString(path, out));
    return Ok(rows);
}

// ---------- StyleLibrary ----------

StyleLibrary& StyleLibrary::get() {
    static StyleLibrary lib;
    static bool loaded = false;
    if (!loaded) { loaded = true; lib.reload(); }
    return lib;
}

std::filesystem::path StyleLibrary::userDir() const {
    auto dir = Mod::get()->getConfigDir() / "styles";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void StyleLibrary::loadDir(std::filesystem::path const& dir, bool builtin) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return;
    std::vector<std::filesystem::path> files;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".json") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (auto& path : files) {
        auto json = file::readJson(path);
        if (!json) {
            log::warn("Auto Deco: could not read style {}: {}", path.filename().string(), json.unwrapErr());
            continue;
        }
        auto id = path.stem().string();
        auto style = Style::fromJson(json.unwrap(), id);
        if (!style) {
            log::warn("Auto Deco: bad style {}: {}", id, style.unwrapErr());
            continue;
        }
        auto s = style.unwrap();
        s.builtin = builtin;
        // user style with same id overrides built-in
        auto it = std::find_if(m_styles.begin(), m_styles.end(), [&](Style const& o) { return o.id == s.id; });
        if (it != m_styles.end()) *it = s;
        else m_styles.push_back(s);
    }
}

void StyleLibrary::reload() {
    m_styles.clear();
    // Geode copies "resources.files" flat into the resources dir; check both layouts.
    loadDir(Mod::get()->getResourcesDir(), true);
    loadDir(Mod::get()->getResourcesDir() / "styles", true);
    loadDir(userDir(), false);
    log::info("Auto Deco: {} styles loaded", m_styles.size());
}

Style const* StyleLibrary::find(std::string const& id) const {
    for (auto& s : m_styles) if (s.id == id) return &s;
    return nullptr;
}

Result<> StyleLibrary::saveUserStyle(Style const& style) {
    auto path = userDir() / (style.id + ".json");
    GEODE_UNWRAP(file::writeString(path, style.toJson().dump()));
    reload();
    return Ok();
}

} // namespace autodeco
