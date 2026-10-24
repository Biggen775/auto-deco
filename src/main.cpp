#include "StylePopup.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Layout.hpp>

using namespace geode::prelude;
using namespace autodeco;

class $modify(AutoDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;
        if (!Mod::get()->getSettingValue<bool>("show-button")) return true;

        auto spr = ButtonSprite::create("Deco", 34, true, "bigFont.fnt", "GJ_button_04.png", 32.f, .45f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AutoDecoEditorUI::onAutoDeco));
        btn->setID("auto-deco-button"_spr);

        // Prefer the Node IDs undo menu (undo / redo / trash, top-left); fall back to a tiny menu
        // of our own next to it.
        if (auto menu = typeinfo_cast<CCMenu*>(this->getChildByID("undo-menu"))) {
            menu->addChild(btn);
            auto size = menu->getContentSize();
            menu->setContentSize({size.width + spr->getContentWidth() + 10.f, size.height});
            menu->updateLayout();
        } else {
            auto menu = CCMenu::create();
            menu->setID("auto-deco-menu"_spr);
            auto win = CCDirector::get()->getWinSize();
            menu->setPosition({win.width / 2, win.height - 20.f});
            menu->addChild(btn);
            this->addChild(menu, 10);
        }
        return true;
    }

    void onAutoDeco(CCObject*) {
        if (auto popup = StylePopup::create(m_editorLayer)) popup->show();
    }
};

$execute {
    // Warm the style library so the first popup opens instantly and bad JSON shows in the log early.
    Loader::get()->queueInMainThread([] { StyleLibrary::get(); });
}
