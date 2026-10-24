#pragma once
// Ordered key/value view of a GD object string ("1,5,2,15,3,45,...;"). No Geode deps.
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace autodeco {

struct ObjString {
    std::vector<std::pair<std::string, std::string>> kv;

    static ObjString parse(std::string const& s) {
        ObjString o;
        std::string cur;
        std::vector<std::string> parts;
        for (char c : s) {
            if (c == ',') { parts.push_back(cur); cur.clear(); }
            else if (c == ';') { break; }
            else cur += c;
        }
        if (!cur.empty()) parts.push_back(cur);
        for (size_t i = 0; i + 1 < parts.size(); i += 2) o.kv.emplace_back(parts[i], parts[i + 1]);
        return o;
    }
    std::string get(std::string const& key, std::string const& def = "") const {
        for (auto& [k, v] : kv) if (k == key) return v;
        return def;
    }
    void set(std::string const& key, std::string const& value) {
        for (auto& [k, v] : kv) if (k == key) { v = value; return; }
        kv.emplace_back(key, value);
    }
    void erase(std::string const& key) {
        kv.erase(std::remove_if(kv.begin(), kv.end(), [&](auto& p) { return p.first == key; }), kv.end());
    }
    std::string str() const {
        std::string out;
        for (auto& [k, v] : kv) {
            if (!out.empty()) out += ',';
            out += k; out += ','; out += v;
        }
        return out;
    }
};

} // namespace autodeco
