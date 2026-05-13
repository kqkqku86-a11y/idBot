#include "game_ui.hpp"

#include <Geode/modify/PlayLayer.hpp>

class $modify(GameUIPlayLayer, PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& g = Global::get();
        auto& ui = Interface::get();

        if (!ui.frameLabel)
            return;

#ifndef GEODE_IS_IOS
        if (g.renderer.recording)
            return;
#endif

        if (g.state != state::none && g.frameLabel) {
            ui.frameLabel->setString(
                fmt::format(
                    "Frame: {}",
                    Global::getCurrentFrame()
                ).c_str()
            );
        }
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        Interface::addLabels(this);
        Interface::addButtons(this);
    }
};

void Interface::addLabels(PlayLayer* pl) {
    auto& ui = Interface::get();

    ui.stateLabel = CCLabelBMFont::create("", "chatFont.fnt");

    ui.stateLabel->setPosition({
        CCDirector::sharedDirector()->getWinSize().width - 6.5f,
        12.f
    });

    ui.stateLabel->setAnchorPoint({1.f, 0.5f});
    ui.stateLabel->setZOrder(300);
    ui.stateLabel->setScale(0.625f);

    pl->addChild(ui.stateLabel);

    ui.frameLabel = CCLabelBMFont::create("", "chatFont.fnt");

    ui.frameLabel->setPosition({6.5f, 12.f});
    ui.frameLabel->setAnchorPoint({0.f, 0.5f});
    ui.frameLabel->setZOrder(300);
    ui.frameLabel->setScale(0.625f);

    pl->addChild(ui.frameLabel);

    Interface::updateLabels();
}

void Interface::addButtons(PlayLayer* pl) {
    auto& ui = Interface::get();

    ui.buttonMenu = CCMenu::create();

    ui.buttonMenu->setPosition({0.f, 0.f});
    ui.buttonMenu->setZOrder(300);

    pl->addChild(ui.buttonMenu);

    ui.stepFrameBtn = Button::createWithSpriteFrameName(
        "GJ_arrow_02_001.png",
        [](auto) {
            auto& g = Global::get();

            if (!g.frameStepper)
                Global::toggleFrameStepper();
            else
                Global::frameStep();
        }
    );

    static_cast<CCSprite*>(
        ui.stepFrameBtn->getDisplayNode()
    )->setFlipX(true);

    ui.stepFrameBtn->setAnchorPoint({0.f, 0.f});

    ui.buttonMenu->addChild(ui.stepFrameBtn);

    ui.backstepFrameBtn = Button::createWithSpriteFrameName(
        "GJ_arrow_02_001.png",
        [](auto) {
            auto& g = Global::get();

            if (!g.frameStepper)
                Global::toggleFrameStepper();
            else
                Global::backstepFrame();
        }
    );

    ui.backstepFrameBtn->setAnchorPoint({0.f, 0.f});

    ui.buttonMenu->addChild(ui.backstepFrameBtn);

    ui.disableStepperBtn = Button::createWithSpriteFrameName(
        "GJ_deleteIcon_001.png",
        [](auto) {
            if (Global::get().frameStepper)
                Global::toggleFrameStepper();
        }
    );

    ui.disableStepperBtn->setAnchorPoint({0.f, 0.f});

    ui.buttonMenu->addChild(ui.disableStepperBtn);

    ui.speedhackBtn = Button::createWithSpriteFrameName(
        "GJ_timeIcon_001.png",
        [](auto) {
            Global::toggleSpeedhack();
        }
    );

    ui.speedhackBtn->setAnchorPoint({0.f, 0.f});

    ui.buttonMenu->addChild(ui.speedhackBtn);

    Interface::updateButtons();
}

void Interface::updateLabels() {
    auto* pl = PlayLayer::get();
    auto* plScene = CCScene::get()->getChildByType<PlayLayer>(0);

    if (!pl && !plScene)
        return;

    auto& g = Global::get();
    auto& ui = Interface::get();

    if (!ui.frameLabel || !ui.stateLabel)
        return;

    if (g.state == state::none || !g.frameLabel) {
        ui.frameLabel->setString("");
    }

    if (g.mod->getSavedValue<bool>("macro_hide_labels")) {
        ui.stateLabel->setString("");
        return;
    }

    std::string labelText;

    if (g.state == state::recording) {
        labelText = "Recording";

        if (g.mod->getSavedValue<bool>(
                "macro_hide_recording_label"
            )) {
            labelText.clear();
        }
    } else if (g.state == state::playing) {
        labelText = "Playing";

        if (g.mod->getSavedValue<bool>(
                "macro_hide_playing_label"
            )) {
            labelText.clear();
        }
    }

#ifndef GEODE_IS_IOS
    if (
        g.renderer.recording &&
        g.mod->getSavedValue<bool>("render_hide_labels")
    ) {
        labelText.clear();

        ui.frameLabel->setString("");
    }
#endif

    ui.stateLabel->setString(labelText.c_str());
}

void Interface::updateButtons() {
    auto* pl = PlayLayer::get();

    if (!pl)
        return;

    auto& g = Global::get();
    auto& ui = Interface::get();

    if (
        !ui.disableStepperBtn ||
        !ui.stepFrameBtn ||
        !ui.backstepFrameBtn ||
        !ui.speedhackBtn
    ) {
        return;
    }

    ui.disableStepperBtn->setPosition(
        ccp(
            g.mod->getSavedValue<float>("button_off_pos_x"),
            g.mod->getSavedValue<float>("button_off_pos_y")
        )
    );

    auto scale = g.mod->getSavedValue<float>("button_off_scale");

    auto* sprite =
        ui.disableStepperBtn->getChildByType<CCSprite>(0);

    sprite->setScale(scale);

    sprite->setOpacity(
        static_cast<GLubyte>(
            g.mod->getSavedValue<float>(
                "button_off_opacity"
            ) * 255.f
        )
    );

    sprite->setAnchorPoint({0.f, 0.f});

    auto size = sprite->getContentSize();

    ui.disableStepperBtn->setContentSize({
        size.width * scale,
        size.height * scale
    });

    ui.stepFrameBtn->setPosition(
        ccp(
            g.mod->getSavedValue<float>(
                "button_advance_frame_pos_x"
            ),
            g.mod->getSavedValue<float>(
                "button_advance_frame_pos_y"
            )
        )
    );

    scale = g.mod->getSavedValue<float>(
        "button_advance_frame_scale"
    );

    sprite = ui.stepFrameBtn->getChildByType<CCSprite>(0);

    sprite->setScale(scale);

    sprite->setOpacity(
        static_cast<GLubyte>(
            g.mod->getSavedValue<float>(
                "button_advance_frame_opacity"
            ) * 255.f
        )
    );

    sprite->setAnchorPoint({0.f, 0.f});

    size = sprite->getContentSize();

    ui.stepFrameBtn->setContentSize({
        size.width * scale,
        size.height * scale
    });

    ui.backstepFrameBtn->setPosition(
        ccp(
            g.mod->getSavedValue<float>(
                "button_backstep_frame_pos_x"
            ),
            g.mod->getSavedValue<float>(
                "button_backstep_frame_pos_y"
            )
        )
    );

    scale = g.mod->getSavedValue<float>(
        "button_backstep_frame_scale"
    );

    sprite = ui.backstepFrameBtn->getChildByType<CCSprite>(0);

    sprite->setScale(scale);

    sprite->setOpacity(
        static_cast<GLubyte>(
            g.mod->getSavedValue<float>(
                "button_backstep_frame_opacity"
            ) * 255.f
        )
    );

    sprite->setAnchorPoint({0.f, 0.f});

    size = sprite->getContentSize();

    ui.backstepFrameBtn->setContentSize({
        size.width * scale,
        size.height * scale
    });

    ui.speedhackBtn->setPosition(
        ccp(
            g.mod->getSavedValue<float>(
                "button_speedhack_pos_x"
            ),
            g.mod->getSavedValue<float>(
                "button_speedhack_pos_y"
            )
        )
    );

    scale = g.mod->getSavedValue<float>(
        "button_speedhack_scale"
    );

    sprite = ui.speedhackBtn->getChildByType<CCSprite>(0);

    sprite->setScale(scale);

    sprite->setOpacity(
        static_cast<GLubyte>(
            g.mod->getSavedValue<float>(
                "button_speedhack_opacity"
            ) * 255.f
        )
    );

    sprite->setAnchorPoint({0.f, 0.f});

    size = sprite->getContentSize();

    ui.speedhackBtn->setContentSize({
        size.width * scale,
        size.height * scale
    });

#ifdef GEODE_IS_DESKTOP
    constexpr bool isDesktop = true;
#else
    constexpr bool isDesktop = false;
#endif

    if (
        (
            g.state != state::recording &&
            !g.mod->getSavedValue<bool>(
                "macro_always_show_buttons"
            )
        ) ||
        isDesktop
    ) {
        ui.disableStepperBtn->setVisible(false);
        ui.stepFrameBtn->setVisible(false);
        ui.backstepFrameBtn->setVisible(false);
        ui.speedhackBtn->setVisible(false);

        return;
    }

    ui.speedhackBtn->setVisible(
        !g.mod->getSavedValue<bool>(
            "macro_hide_speedhack"
        )
    );

    if (g.mod->getSavedValue<bool>("macro_hide_stepper")) {
        ui.disableStepperBtn->setVisible(false);
        ui.stepFrameBtn->setVisible(false);
        ui.backstepFrameBtn->setVisible(false);
    } else {
        ui.stepFrameBtn->setVisible(true);
        ui.backstepFrameBtn->setVisible(true);
        ui.disableStepperBtn->setVisible(g.frameStepper);
    }
}
