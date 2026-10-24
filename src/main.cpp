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

        // Our own little menu to the right of the undo / redo / trash menu. We don't touch that
        // menu's size: widening it pushes GD's own undo button off the left edge of the screen.
        auto ownMenu = CCMenu::create();
        ownMenu->setID("auto-deco-menu"_spr);
        ownMenu->setContentSize(btn->getContentSize());
        ownMenu->ignoreAnchorPointForPosition(false);
        btn->setPosition({btn->getContentWidth() / 2, btn->getContentHeight() / 2});
        ownMenu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        CCPoint pos{win.width / 2 - 150.f, win.height - 22.f};
        if (auto menu = typeinfo_cast<CCMenu*>(this->getChildByID("undo-menu"))) {
            auto box = menu->boundingBox();
            pos = CCPoint(box.getMaxX() + 12.f + btn->getContentWidth() / 2, box.getMidY());
        }
        ownMenu->setPosition(pos);
        this->addChild(ownMenu, 10);
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
