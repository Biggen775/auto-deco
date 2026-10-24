#pragma once
#include "Decorator.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

namespace autodeco {

class StylePopup : public geode::Popup {
public:
    static StylePopup* create(LevelEditorLayer* layer);

protected:
    bool init(LevelEditorLayer* layer);

    LevelEditorLayer* m_layer = nullptr;
    geode::ScrollLayer* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_info = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    CCMenuItemToggler* m_selOnly = nullptr;
    CCMenuItemToggler* m_scatter = nullptr;
    CCMenuItemToggler* m_recolor = nullptr;
    CCMenuItemToggler* m_hazards = nullptr;
    std::string m_selectedId;

    void rebuildList();
    void setStatus(std::string const& text, bool error = false);
    void onPick(cocos2d::CCObject* sender);
    void onApply(cocos2d::CCObject*);
    void onUndo(cocos2d::CCObject*);
    void onSave(cocos2d::CCObject*);
    void onReload(cocos2d::CCObject*);
    void onOpenFolder(cocos2d::CCObject*);
    void onDumpIds(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*) {}
};

// Tiny "enter a name" popup used by Save Style.
class NamePopup : public geode::Popup {
public:
    static NamePopup* create(std::string const& title, geode::Function<void(std::string const&)> onDone);
protected:
    bool init(std::string const& title, geode::Function<void(std::string const&)> onDone);
    geode::TextInput* m_input = nullptr;
    geode::Function<void(std::string const&)> m_onDone;
    void onOk(cocos2d::CCObject*);
};

} // namespace autodeco
