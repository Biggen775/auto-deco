#include "StylePopup.hpp"
#include "OptionsPopup.hpp"
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

namespace autodeco {

static constexpr float W = 400.f, H = 270.f;
static constexpr float LIST_W = 170.f, LIST_H = 185.f;

StylePopup* StylePopup::create(LevelEditorLayer* layer) {
    auto ret = new StylePopup();
    if (ret && ret->init(layer)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool StylePopup::init(LevelEditorLayer* layer) {
    if (!Popup::init(W, H)) return false;
    m_layer = layer;
    this->setTitle("Auto Deco");

    auto center = m_mainLayer->getContentSize() / 2;

    // ----- left: style list
    auto listBg = CCScale9Sprite::create("square02b_001.png");
    listBg->setContentSize({LIST_W, LIST_H});
    listBg->setColor({0, 0, 0});
    listBg->setOpacity(90);
    listBg->setPosition({20.f + LIST_W / 2, center.height - 5.f});
    m_mainLayer->addChild(listBg);

    m_list = ScrollLayer::create({LIST_W - 6.f, LIST_H - 6.f});
    m_list->setPosition({23.f, center.height - 5.f - (LIST_H - 6.f) / 2});
    m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(3.f));
    m_mainLayer->addChild(m_list);

    // ----- right: toggles
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu);

    float rx = 20.f + LIST_W + 22.f;
    float ry = center.height + LIST_H / 2 - 18.f;
    auto addToggle = [&](const char* text, bool on, CCMenuItemToggler*& out) {
        out = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(StylePopup::onToggle), .55f);
        out->setPosition({rx, ry});
        out->toggle(on);
        menu->addChild(out);
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(.38f);
        lbl->setAnchorPoint({0, .5f});
        lbl->setPosition({rx + 16.f, ry});
        m_mainLayer->addChild(lbl);
        ry -= 26.f;
    };
    addToggle("Selection only", false, m_selOnly);
    addToggle("Background deco", true, m_scatter);
    addToggle("Recolor level", true, m_recolor);
    addToggle("Restyle hazards", true, m_hazards);

    m_info = CCLabelBMFont::create("", "chatFont.fnt");
    m_info->setScale(.55f);
    m_info->setAnchorPoint({0, 1});
    m_info->setPosition({rx - 12.f, ry + 4.f});
    m_info->setWidth(W - rx - 10.f);
    m_mainLayer->addChild(m_info);

    m_customInfo = CCLabelBMFont::create("", "chatFont.fnt");
    m_customInfo->setScale(.5f);
    m_customInfo->setAnchorPoint({0, 1});
    m_customInfo->setPosition({rx - 12.f, ry - 30.f});
    m_customInfo->setWidth(W - rx - 10.f);
    m_customInfo->setColor({170, 220, 255});
    m_mainLayer->addChild(m_customInfo);

    // ----- buttons
    auto btnMenu = CCMenu::create();
    btnMenu->setPosition({0, 0});
    m_mainLayer->addChild(btnMenu);

    auto applySpr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", .8f);
    auto applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(StylePopup::onApply));
    applyBtn->setPosition({W - 68.f, 34.f});
    btnMenu->addChild(applyBtn);

    auto undoSpr = ButtonSprite::create("Undo", "bigFont.fnt", "GJ_button_06.png", .6f);
    auto undoBtn = CCMenuItemSpriteExtra::create(undoSpr, this, menu_selector(StylePopup::onUndo));
    undoBtn->setPosition({W - 138.f, 34.f});
    btnMenu->addChild(undoBtn);

    auto custSpr = ButtonSprite::create("Customize", "bigFont.fnt", "GJ_button_04.png", .45f);
    auto custBtn = CCMenuItemSpriteExtra::create(custSpr, this, menu_selector(StylePopup::onCustomize));
    custBtn->setPosition({W - 142.f, H - 20.f});
    btnMenu->addChild(custBtn);

    auto saveSpr = ButtonSprite::create("Save selection as style", "bigFont.fnt", "GJ_button_04.png", .45f);
    auto saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(StylePopup::onSave));
    saveBtn->setPosition({20.f + LIST_W / 2, 34.f});
    btnMenu->addChild(saveBtn);

    // Small icon buttons; fall back to text if a sprite is missing so we never deref null.
    auto iconOrText = [](const char* frame, const char* text) -> CCSprite* {
        if (auto s = CCSprite::createWithSpriteFrameName(frame)) { s->setScale(.5f); return s; }
        if (auto s = CCSprite::create(frame)) { s->setScale(.5f); return s; }
        return ButtonSprite::create(text, "bigFont.fnt", "GJ_button_04.png", .4f);
    };
    auto reloadBtn = CCMenuItemSpriteExtra::create(iconOrText("geode.loader/reload.png", "R"), this, menu_selector(StylePopup::onReload));
    reloadBtn->setPosition({W - 20.f, H - 20.f});
    btnMenu->addChild(reloadBtn);

    auto folderBtn = CCMenuItemSpriteExtra::create(iconOrText("gj_folderBtn_001.png", "F"), this, menu_selector(StylePopup::onOpenFolder));
    folderBtn->setPosition({W - 46.f, H - 20.f});
    btnMenu->addChild(folderBtn);

    auto dumpSpr = ButtonSprite::create("IDs", "bigFont.fnt", "GJ_button_04.png", .4f);
    auto dumpBtn = CCMenuItemSpriteExtra::create(dumpSpr, this, menu_selector(StylePopup::onDumpIds));
    dumpBtn->setPosition({W - 80.f, H - 20.f});
    btnMenu->addChild(dumpBtn);

    m_status = CCLabelBMFont::create("", "chatFont.fnt");
    m_status->setScale(.5f);
    m_status->setPosition({center.width, 10.f});
    m_mainLayer->addChild(m_status);

    auto& styles = StyleLibrary::get().styles();
    if (!styles.empty()) m_selectedId = styles.front().id;
    rebuildList();
    refreshCustomInfo();
    Decorator deco(m_layer);
    if (deco.hasBackup()) setStatus("This level has Auto Deco applied. Apply again restyles from the original layout.");
    return true;
}

void StylePopup::setStatus(std::string const& text, bool error) {
    m_status->setString(text.c_str());
    m_status->setColor(error ? ccColor3B{255, 90, 90} : ccColor3B{200, 200, 200});
}

void StylePopup::rebuildList() {
    m_list->m_contentLayer->removeAllChildren();
    auto& styles = StyleLibrary::get().styles();
    float rowW = LIST_W - 10.f;
    for (auto& s : styles) {
        bool sel = s.id == m_selectedId;
        auto spr = ButtonSprite::create(s.name.c_str(), (int)rowW, true, "bigFont.fnt", sel ? "GJ_button_02.png" : "GJ_button_05.png", 24.f, .45f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(StylePopup::onPick));
        btn->setUserObject("id"_spr, CCString::create(s.id));
        auto menu = CCMenu::create();
        menu->setContentSize({rowW, 26.f});
        menu->setPosition({0, 0});
        btn->setPosition({rowW / 2, 13.f});
        menu->addChild(btn);
        m_list->m_contentLayer->addChild(menu);
    }
    if (styles.empty()) {
        auto lbl = CCLabelBMFont::create("No styles found", "bigFont.fnt");
        lbl->setScale(.4f);
        m_list->m_contentLayer->addChild(lbl);
    }
    m_list->m_contentLayer->updateLayout();
    m_list->scrollToTop();

    if (auto s = StyleLibrary::get().find(m_selectedId)) {
        std::string txt = s->description;
        if (txt.empty()) txt = s->builtin ? "Built-in style" : "User style";
        txt += fmt::format("\n{} block pieces, {} scatter rules",
            std::count_if(s->blocks.begin(), s->blocks.end(), [](auto& b) { return b.has_value(); }), s->scatter.size());
        m_info->setString(txt.c_str());
    } else {
        m_info->setString("");
    }
}

void StylePopup::onPick(CCObject* sender) {
    auto node = static_cast<CCNode*>(sender);
    if (auto id = static_cast<CCString*>(node->getUserObject("id"_spr))) {
        m_selectedId = id->getCString();
        rebuildList();
    }
}

void StylePopup::onApply(CCObject*) {
    auto style = StyleLibrary::get().find(m_selectedId);
    if (!style) { setStatus("Pick a style first", true); return; }
    ApplyOptions opts;
    opts.selectionOnly = m_selOnly->isToggled();
    opts.scatter = m_scatter->isToggled();
    opts.recolor = m_recolor->isToggled();
    opts.hazards = m_hazards->isToggled();
    opts.custom = Customize::load();
    Decorator deco(m_layer);
    auto r = deco.apply(*style, opts);
    if (!r) { setStatus(r.unwrapErr(), true); return; }
    auto rep = r.unwrap();
    Notification::create(fmt::format("{}: {} blocks, {} hazards, {} deco, {} fx", style->name, rep.blocksReplaced, rep.hazardsTouched, rep.scatterAdded, rep.triggersAdded), NotificationIcon::Success)->show();
    this->onClose(nullptr);
}

void StylePopup::onUndo(CCObject*) {
    Decorator deco(m_layer);
    auto r = deco.undo();
    if (!r) { setStatus(r.unwrapErr(), true); return; }
    if (!r.unwrap()) { setStatus("Nothing to undo for this level", true); return; }
    Notification::create("Auto Deco removed, layout restored", NotificationIcon::Success)->show();
    this->onClose(nullptr);
}

void StylePopup::onSave(CCObject*) {
    auto ui = EditorUI::get();
    auto sel = ui ? ui->getSelectedObjects() : nullptr;
    if (!sel || sel->count() == 0) { setStatus("Select a sample first (blocks + spikes + deco)", true); return; }
    NamePopup::create("Style name", [this](std::string const& name) {
        if (name.empty()) return;
        Decorator deco(m_layer);
        auto r = deco.learnFromSelection(name);
        if (!r) { setStatus(r.unwrapErr(), true); return; }
        auto style = r.unwrap();
        auto w = StyleLibrary::get().saveUserStyle(style);
        if (!w) { setStatus(w.unwrapErr(), true); return; }
        m_selectedId = style.id;
        rebuildList();
        setStatus(fmt::format("Saved '{}' to your styles folder", style.name));
    })->show();
}

void StylePopup::onReload(CCObject*) {
    StyleLibrary::get().reload();
    rebuildList();
    setStatus(fmt::format("{} styles loaded", StyleLibrary::get().styles().size()));
}

void StylePopup::onOpenFolder(CCObject*) {
    file::openFolder(StyleLibrary::get().userDir());
}

void StylePopup::onCustomize(CCObject*) {
    OptionsPopup::create([this] { this->refreshCustomInfo(); })->show();
}

void StylePopup::refreshCustomInfo() {
    auto c = Customize::load();
    float sc = (float)Mod::get()->getSettingValue<double>("scatter-multiplier");
    std::string txt = fmt::format("Outline {} | Glow {}% | Deco {}%", Customize::outlineName(c.outline),
        (int)std::lround(c.glow * 100), (int)std::lround(sc * 100));
    txt += fmt::format("\nPulse {} | Shader {} | Spin {}", Customize::pulseName(c.pulse), Customize::shaderName(c.shader),
        c.spin < 0 ? "Style" : (c.spin ? "On" : "Off"));
    if (c.useAccent) txt += fmt::format(" | Accent {}", colorToHex(c.accent));
    if (!c.detail) txt += " | no detail";
    if (!c.hazardGlow) txt += " | no hazard glow";
    m_customInfo->setString(txt.c_str());
}

void StylePopup::onDumpIds(CCObject*) {
    auto path = Mod::get()->getConfigDir() / "object-table.tsv";
    auto r = dumpObjectTable(path);
    if (!r) { setStatus(r.unwrapErr(), true); return; }
    setStatus(fmt::format("Wrote {} object IDs to object-table.tsv (config folder)", r.unwrap()));
}

// ---------------------------------------------------------------- NamePopup

NamePopup* NamePopup::create(std::string const& title, Function<void(std::string const&)> onDone) {
    auto ret = new NamePopup();
    if (ret && ret->init(title, std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool NamePopup::init(std::string const& title, Function<void(std::string const&)> onDone) {
    if (!Popup::init(240.f, 120.f)) return false;
    m_onDone = std::move(onDone);
    this->setTitle(title);
    auto center = m_mainLayer->getContentSize() / 2;

    m_input = TextInput::create(200.f, "My style");
    m_input->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
    m_input->setMaxCharCount(32);
    m_input->setPosition({center.width, center.height + 5.f});
    m_mainLayer->addChild(m_input);

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu);
    auto okSpr = ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", .7f);
    auto ok = CCMenuItemSpriteExtra::create(okSpr, this, menu_selector(NamePopup::onOk));
    ok->setPosition({center.width, 24.f});
    menu->addChild(ok);
    return true;
}

void NamePopup::onOk(CCObject*) {
    auto name = std::string(m_input->getString());
    auto done = std::move(m_onDone); // onClose may free this popup
    this->onClose(nullptr);
    if (done) done(name);
}

} // namespace autodeco
