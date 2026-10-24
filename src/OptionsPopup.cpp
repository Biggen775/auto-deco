#include "OptionsPopup.hpp"
#include <Geode/ui/ColorPickPopup.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace autodeco {

static constexpr float PW = 380.f, PH = 300.f;

OptionsPopup* OptionsPopup::create(Function<void()> onChanged) {
    auto ret = new OptionsPopup();
    if (ret && ret->init(std::move(onChanged))) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

static CCSprite* arrowSprite(bool right) {
    CCSprite* s = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    if (!s) s = CCSprite::create("GJ_arrow_01_001.png");
    if (!s) {
        return ButtonSprite::create(right ? ">" : "<", "bigFont.fnt", "GJ_button_04.png", .5f);
    }
    s->setScale(.6f);
    if (right) s->setFlipX(true);
    return s;
}

bool OptionsPopup::init(Function<void()> onChanged) {
    if (!Popup::init(PW, PH)) return false;
    m_onChanged = std::move(onChanged);
    m_c = Customize::load();
    this->setTitle("Customize deco");

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu);

    float labelX = 22.f;
    float y = PH - 48.f;
    const float ROW = 29.f;

    auto rowLabel = [&](const char* text, float yy) {
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(.38f);
        l->setAnchorPoint({0, .5f});
        l->setPosition({labelX, yy});
        m_mainLayer->addChild(l);
    };
    // A "< value >" chooser. Left arrow has tag -1, right arrow tag +1.
    auto chooser = [&](float yy, SEL_MenuHandler sel, CCLabelBMFont*& out) {
        float cx = PW - 110.f;
        auto l = CCMenuItemSpriteExtra::create(arrowSprite(false), this, sel);
        l->setTag(-1); l->setPosition({cx - 62.f, yy}); menu->addChild(l);
        auto r = CCMenuItemSpriteExtra::create(arrowSprite(true), this, sel);
        r->setTag(1); r->setPosition({cx + 62.f, yy}); menu->addChild(r);
        out = CCLabelBMFont::create("", "bigFont.fnt");
        out->setScale(.42f);
        out->setPosition({cx, yy});
        m_mainLayer->addChild(out);
    };
    auto slider = [&](float yy, SEL_MenuHandler sel, Slider*& out, CCLabelBMFont*& lbl) {
        out = Slider::create(this, sel, .65f);
        out->setPosition({PW - 120.f, yy});
        m_mainLayer->addChild(out);
        lbl = CCLabelBMFont::create("", "bigFont.fnt");
        lbl->setScale(.38f);
        lbl->setPosition({PW - 34.f, yy});
        m_mainLayer->addChild(lbl);
    };
    auto toggle = [&](float xx, float yy, const char* text, SEL_MenuHandler sel, CCMenuItemToggler*& out) {
        out = CCMenuItemToggler::createWithStandardSprites(this, sel, .5f);
        out->setPosition({xx, yy});
        menu->addChild(out);
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(.34f);
        l->setAnchorPoint({0, .5f});
        l->setPosition({xx + 14.f, yy});
        m_mainLayer->addChild(l);
    };

    rowLabel("Outline", y);      chooser(y, menu_selector(OptionsPopup::onOutline), m_outlineLbl); y -= ROW;
    rowLabel("Glow", y);         slider(y, menu_selector(OptionsPopup::onGlow), m_glow, m_glowLbl); y -= ROW;
    rowLabel("Background deco", y); slider(y, menu_selector(OptionsPopup::onScatter), m_scatter, m_scatterLbl); y -= ROW;
    toggle(labelX + 8.f, y, "Inner detail", menu_selector(OptionsPopup::onDetail), m_detail);
    toggle(PW / 2 + 10.f, y, "Hazard glow", menu_selector(OptionsPopup::onHazardGlow), m_hazardGlow); y -= ROW;
    toggle(labelX + 8.f, y, "Accent colour", menu_selector(OptionsPopup::onUseAccent), m_useAccent);
    {
        m_swatch = CCSprite::create("square02b_001.png");
        m_swatch->setScale(.5f);
        auto btn = CCMenuItemSpriteExtra::create(m_swatch, this, menu_selector(OptionsPopup::onPickAccent));
        btn->setPosition({PW / 2 + 40.f, y});
        menu->addChild(btn);
        auto l = CCLabelBMFont::create("tap to pick", "bigFont.fnt");
        l->setScale(.3f);
        l->setAnchorPoint({0, .5f});
        l->setPosition({PW / 2 + 62.f, y});
        m_mainLayer->addChild(l);
    }
    y -= ROW;
    rowLabel("Pulse (beat)", y);  chooser(y, menu_selector(OptionsPopup::onPulse), m_pulseLbl); y -= ROW;
    rowLabel("Shader", y);        chooser(y, menu_selector(OptionsPopup::onShader), m_shaderLbl); y -= ROW;
    rowLabel("Spinning deco", y); chooser(y, menu_selector(OptionsPopup::onSpin), m_spinLbl); y -= ROW;

    auto resetSpr = ButtonSprite::create("Reset", "bigFont.fnt", "GJ_button_06.png", .5f);
    auto resetBtn = CCMenuItemSpriteExtra::create(resetSpr, this, menu_selector(OptionsPopup::onReset));
    resetBtn->setPosition({PW / 2, 22.f});
    menu->addChild(resetBtn);

    auto hint = CCLabelBMFont::create("\"Style\" = use what the style says. Applies on the next Apply.", "chatFont.fnt");
    hint->setScale(.5f);
    hint->setPosition({PW / 2, 42.f});
    hint->setColor({180, 180, 180});
    m_mainLayer->addChild(hint);

    refresh();
    return true;
}

void OptionsPopup::refresh() {
    m_outlineLbl->setString(Customize::outlineName(m_c.outline));
    m_pulseLbl->setString(Customize::pulseName(m_c.pulse));
    m_shaderLbl->setString(Customize::shaderName(m_c.shader));
    m_spinLbl->setString(m_c.spin < 0 ? "Style" : (m_c.spin ? "On" : "Off"));
    m_glow->setValue(std::clamp(m_c.glow / 2.f, 0.f, 1.f));
    m_glowLbl->setString(fmt::format("{}%", (int)std::lround(m_c.glow * 100)).c_str());
    float sc = (float)Mod::get()->getSettingValue<double>("scatter-multiplier");
    m_scatter->setValue(std::clamp(sc / 3.f, 0.f, 1.f));
    m_scatterLbl->setString(fmt::format("{}%", (int)std::lround(sc * 100)).c_str());
    m_detail->toggle(m_c.detail);
    m_hazardGlow->toggle(m_c.hazardGlow);
    m_useAccent->toggle(m_c.useAccent);
    m_swatch->setColor(m_c.accent.cc());
}

void OptionsPopup::persist() {
    m_c.save();
    if (m_onChanged) m_onChanged();
}

static int cycle(int v, int lo, int hi, int dir) {
    v += dir;
    if (v > hi) v = lo;
    if (v < lo) v = hi;
    return v;
}

void OptionsPopup::onOutline(CCObject* s) { m_c.outline = cycle(m_c.outline, 0, 3, static_cast<CCNode*>(s)->getTag()); refresh(); persist(); }
void OptionsPopup::onPulse(CCObject* s)   { m_c.pulse = cycle(m_c.pulse, -1, 2, static_cast<CCNode*>(s)->getTag()); refresh(); persist(); }
void OptionsPopup::onShader(CCObject* s)  { m_c.shader = cycle(m_c.shader, -1, 4, static_cast<CCNode*>(s)->getTag()); refresh(); persist(); }
void OptionsPopup::onSpin(CCObject* s)    { m_c.spin = cycle(m_c.spin, -1, 1, static_cast<CCNode*>(s)->getTag()); refresh(); persist(); }

void OptionsPopup::onGlow(CCObject*) {
    m_c.glow = std::round(m_glow->getValue() * 20.f) / 10.f; // 0..2 in 0.1 steps
    m_glowLbl->setString(fmt::format("{}%", (int)std::lround(m_c.glow * 100)).c_str());
    persist();
}
void OptionsPopup::onScatter(CCObject*) {
    float v = std::round(m_scatter->getValue() * 30.f) / 10.f; // 0..3
    Mod::get()->setSettingValue<double>("scatter-multiplier", v);
    m_scatterLbl->setString(fmt::format("{}%", (int)std::lround(v * 100)).c_str());
    if (m_onChanged) m_onChanged();
}
void OptionsPopup::onDetail(CCObject*)     { m_c.detail = !m_detail->isToggled(); persist(); }
void OptionsPopup::onHazardGlow(CCObject*) { m_c.hazardGlow = !m_hazardGlow->isToggled(); persist(); }
void OptionsPopup::onUseAccent(CCObject*)  { m_c.useAccent = !m_useAccent->isToggled(); persist(); }

void OptionsPopup::onPickAccent(CCObject*) {
    auto popup = ColorPickPopup::create(m_c.accent.cc());
    popup->setCallback([this](ccColor4B const& c) {
        m_c.accent = RGB{c.r, c.g, c.b};
        m_c.useAccent = true;
        refresh();
        persist();
    });
    popup->show();
}

void OptionsPopup::onReset(CCObject*) {
    m_c = Customize();
    Mod::get()->setSettingValue<double>("scatter-multiplier", 1.0);
    refresh();
    persist();
}

void OptionsPopup::onClose(CCObject* s) {
    persist();
    Popup::onClose(s);
}

} // namespace autodeco
