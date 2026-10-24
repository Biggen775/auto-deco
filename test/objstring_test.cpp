#include "ObjString.hpp"
#include <cassert>
#include <cstdio>

using namespace autodeco;

int main() {
    auto o = ObjString::parse("1,1,2,45,3,15,6,90,21,1004,57,3.7,32,1.5;");
    assert(o.get("1") == "1");
    assert(o.get("6") == "90");
    assert(o.get("57") == "3.7");
    assert(o.get("99", "x") == "x");
    o.set("1", "5");
    o.set("6", "180");
    o.erase("32");
    o.set("57", o.get("57") + ".42");
    auto s = o.str();
    printf("%s\n", s.c_str());
    assert(s == "1,5,2,45,3,15,6,180,21,1004,57,3.7.42");
    auto e = ObjString::parse("");
    assert(e.kv.empty());
    puts("objstring ok");
    return 0;
}
