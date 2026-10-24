// g++ -std=c++20 -I../src classifier_test.cpp -o t && ./t
#include "Classifier.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace autodeco;

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void expect(unsigned open, unsigned diag, Role role, int rot, size_t count = 1) {
    auto p = classify(open, diag);
    CHECK(p.size() == count);
    CHECK(p[0].role == role);
    CHECK(p[0].rotation == rot);
    if (p.size() != count || p[0].role != role || p[0].rotation != rot)
        printf("   open=%u diag=%u -> %s/%d (n=%zu)\n", open, diag, roleName(p[0].role), p[0].rotation, p.size());
}

// Render a tiny map and print the result as ascii so the output can be eyeballed.
static void renderMap(const char* rows[], int h, int w) {
    auto solid = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        return rows[y][x] == '#';
    };
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!solid(x, y)) { printf("   .   "); continue; }
            unsigned open = 0, diag = 0;
            if (!solid(x, y - 1)) open |= T;
            if (!solid(x + 1, y)) open |= R;
            if (!solid(x, y + 1)) open |= B;
            if (!solid(x - 1, y)) open |= L;
            if (!solid(x - 1, y - 1)) diag |= TL;
            if (!solid(x + 1, y - 1)) diag |= TR;
            if (!solid(x + 1, y + 1)) diag |= BR;
            if (!solid(x - 1, y + 1)) diag |= BL;
            auto p = classify(open, diag);
            char buf[16];
            snprintf(buf, sizeof buf, "%c%03d%s", roleName(p[0].role)[0], p[0].rotation, p.size() > 1 ? "+" : " ");
            printf(" %s ", buf);
        }
        printf("\n");
    }
}

int main() {
    // Single
    expect(T | R | B | L, 0, Role::Single, 0);
    // Edges
    expect(T, 0, Role::Edge, 0);
    expect(R, 0, Role::Edge, 90);
    expect(B, 0, Role::Edge, 180);
    expect(L, 0, Role::Edge, 270);
    // Corners
    expect(T | L, 0, Role::Corner, 0);
    expect(T | R, 0, Role::Corner, 90);
    expect(R | B, 0, Role::Corner, 180);
    expect(B | L, 0, Role::Corner, 270);
    // Pillars
    expect(L | R, 0, Role::Pillar, 0);
    expect(T | B, 0, Role::Pillar, 90);
    // Caps (open side)
    expect(L | T | R, 0, Role::Cap, 0);   // open bottom
    expect(T | R | B, 0, Role::Cap, 90);  // open left
    expect(R | B | L, 0, Role::Cap, 180); // open top
    expect(B | L | T, 0, Role::Cap, 270); // open right
    // Inner + notches
    expect(0, 0, Role::Inner, 0, 1);
    expect(0, TL, Role::Inner, 0, 2);
    expect(0, TL | BR, Role::Inner, 0, 3);
    {
        auto p = classify(0, BR);
        CHECK(p.size() == 2 && p[1].role == Role::InnerCorner && p[1].rotation == 180);
    }

    const char* rows[] = {
        "..####..",
        "..####..",
        "####..##",
        "####..##",
        "#......#",
        "########",
    };
    renderMap(rows, 6, 8);

    printf(fails ? "FAILED (%d)\n" : "all ok\n", fails);
    return fails ? 1 : 0;
}
