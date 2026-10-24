#pragma once
#include "Decorator.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

namespace autodeco {

// "Customize" panel: per-user tweaks layered on top of any style (outline, glow, detail,
// scatter density, accent colour, pulse, shader, spinning deco). Saved between sessions.
class OptionsPopup : public geode::Popup {
public:
    static OptionsPopup* create(geode::Function<void()> onChanged);

protected:
    bool init(geode::Function<void()> onChanged);

    Customize m_c;
    geode::Function<void()> m_onChanged;
    Slider* m_glow = nullptr;
    Slider* m_scatter = nullptr;
    cocos2d::CCLabelBMFont* m_glowLbl = nullptr;
    cocos2d::CCLabelBMFont* m_scatterLbl = nullptr;
    cocos2d::CCLabelBMFont* m_outlineLbl = nullptr;
    cocos2d::CCLabelBMFont* m_pulseLbl = nullptr;
    cocos2d::CCLabelBMFont* m_shaderLbl = nullptr;
    cocos2d::CCLabelBMFont* m_spinLbl = nullptr;
    cocos2d::CCSprite* m_swatch = nullptr;
    CCMenuItemToggler* m_detail = nullptr;
    CCMenuItemToggler* m_hazardGlow = nullptr;
    CCMenuItemToggler* m_useAccent = nullptr;

    void refresh();
    void persist();
    void onOutline(cocos2d::CCObject* sender);
    void onPulse(cocos2d::CCObject* sender);
    void onShader(cocos2d::CCObject* sender);
    void onSpin(cocos2d::CCObject* sender);
    void onGlow(cocos2d::CCObject*);
    void onScatter(cocos2d::CCObject*);
    void onDetail(cocos2d::CCObject*);
    void onHazardGlow(cocos2d::CCObject*);
    void onUseAccent(cocos2d::CCObject*);
    void onPickAccent(cocos2d::CCObject*);
    void onReset(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*) override;
};

} // namespace autodeco
