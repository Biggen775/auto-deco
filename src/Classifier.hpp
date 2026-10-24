#pragma once
// Pure autotile logic. No Geode / cocos includes so it can be unit-tested on its own.
//
// Reference orientation of every role (matches GD's 1.0 grid block family, IDs 1-7 + 83):
//   Edge        outline on TOP                       (square_02, ID 2)
//   Corner      outline on TOP + LEFT                (square_03, ID 3)
//   InnerCorner small notch at TOP-LEFT              (square_04, ID 4)
//   Inner       no outline                           (square_05, ID 5)
//   Cap         outline LEFT + TOP + RIGHT, open at BOTTOM (square_06, ID 6)
//   Pillar      outline LEFT + RIGHT, open TOP/BOTTOM      (square_07, ID 7)
//   Single      outline on all four sides            (square_08, ID 83)
// Other orientations are produced by rotating clockwise (GD object key 6).

#include <cstdint>
#include <vector>

namespace autodeco {

enum class Role : uint8_t { Inner, Edge, Corner, InnerCorner, Cap, Pillar, Single, COUNT };

inline const char* roleName(Role r) {
    switch (r) {
        case Role::Inner: return "inner";
        case Role::Edge: return "edge";
        case Role::Corner: return "corner";
        case Role::InnerCorner: return "innerCorner";
        case Role::Cap: return "cap";
        case Role::Pillar: return "pillar";
        case Role::Single: return "single";
        default: return "?";
    }
}

// Side bits: a bit is SET when that side is OPEN (no solid neighbour).
enum Side : unsigned { T = 1, R = 2, B = 4, L = 8 };
// Diagonal bits: SET when that diagonal neighbour is open.
enum Diag : unsigned { TL = 1, TR = 2, BR = 4, BL = 8 };

struct Placement {
    Role role;
    int rotation; // degrees clockwise, one of 0/90/180/270
};

// Returns the pieces to place in one cell. First entry is the base piece; any further
// entries are InnerCorner notches stacked on top (only emitted for fully enclosed cells).
inline std::vector<Placement> classify(unsigned open, unsigned openDiag) {
    std::vector<Placement> out;
    unsigned n = (open & T ? 1 : 0) + (open & R ? 1 : 0) + (open & B ? 1 : 0) + (open & L ? 1 : 0);

    switch (n) {
        case 0: {
            out.push_back({Role::Inner, 0});
            // A notch is needed where a diagonal is open but both sides next to it are solid.
            if (openDiag & TL) out.push_back({Role::InnerCorner, 0});
            if (openDiag & TR) out.push_back({Role::InnerCorner, 90});
            if (openDiag & BR) out.push_back({Role::InnerCorner, 180});
            if (openDiag & BL) out.push_back({Role::InnerCorner, 270});
            break;
        }
        case 1: {
            int rot = (open & T) ? 0 : (open & R) ? 90 : (open & B) ? 180 : 270;
            out.push_back({Role::Edge, rot});
            break;
        }
        case 2: {
            if ((open & (T | B)) == (T | B)) out.push_back({Role::Pillar, 90});
            else if ((open & (L | R)) == (L | R)) out.push_back({Role::Pillar, 0});
            else if ((open & (T | L)) == (T | L)) out.push_back({Role::Corner, 0});
            else if ((open & (T | R)) == (T | R)) out.push_back({Role::Corner, 90});
            else if ((open & (R | B)) == (R | B)) out.push_back({Role::Corner, 180});
            else out.push_back({Role::Corner, 270}); // B | L
            break;
        }
        case 3: {
            // Cap is open on exactly one side. Reference: open at BOTTOM.
            unsigned closed = (~open) & (T | R | B | L);
            int rot = (closed & B) ? 0 : (closed & L) ? 90 : (closed & T) ? 180 : 270;
            out.push_back({Role::Cap, rot});
            break;
        }
        default:
            out.push_back({Role::Single, 0});
            break;
    }
    return out;
}

// Inverse of classify for a single base piece: given a piece's role and the rotation it was
// placed at, return the rotation it would have in the reference orientation. Used when
// learning a style from a hand-built sample: offset = placedRotation - expectedRotation.
inline int normalizeRotation(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    return deg;
}

} // namespace autodeco
