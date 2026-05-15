#include "../includes.hpp"
#include "../global.hpp"
#include "../practice_fixes/practice_fixes.hpp"
#include "../ui/record_layer.hpp"

#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/GameLevelOptionsLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

const std::unordered_set<int> shaderIDs = {2904,2905,2907,2909,2910,2911,2912,2913,2914,2915,2916,2917,2919,2920,2921,2922,2923,2924};

class $modify(CCScheduler) {

    void update(float dt) {
        auto& g = Global::get();

        if (g.schedulerUpdating)
            return CCScheduler::update(dt);

        if (g.state == state::playing && !g.tpsEnabled && g.macro.framerate != 240.f)
            g.setTpsEnabled(true);

        if (g.state == state::none && !g.speedhackEnabled && !g.tpsEnabled && !g.lockDelta &&
            !g.frameStepper) {
            if (g.currentPitch != 1.f)
                Global::updatePitch(1.f);

            return CCScheduler::update(dt);
        }
#ifndef GEODE_IS_IOS
        if (g.renderer.recording) {
            if (g.currentPitch != 1.f)
                Global::updatePitch(1.f);

            return CCScheduler::update(dt);
        }
#endif

        float speedhack = 1.f;

        if (g.speedhackEnabled && !g.frameStepper) {
            std::string speedhackValue = g.mod->getSavedValue<std::string>("macro_speedhack");

            if (speedhackValue != "0.0" && speedhackValue != "") {
                speedhack = geode::utils::numFromString<float>(speedhackValue).unwrap();
                float decimals = speedhack - static_cast<int>(speedhack);

                float closest = safeValues[0];
                float minDiff = std::abs(decimals - closest);

                for (float value : safeValues) {
                    if (speedhackValue == "1.0" || g.state == state::none) {
                        closest = decimals;
                        break;
                    }

                    float diff = std::abs(decimals - value);

                    if (diff < minDiff) {
                        minDiff = diff;
                        closest = value;
                    }
                }

                speedhack = static_cast<int>(speedhack) + closest;
            }

            Global::updatePitch(speedhack);
        }

        if (speedhack != 1.f && PlayLayer::get())
            g.safeMode = true;

        auto* pl = PlayLayer::get();
        if (!pl || pl->m_isPaused) {
            g.schedulerOverflow = 0.0;
            return CCScheduler::update(dt * speedhack);
        }

        double physicsDt = 1.0 / static_cast<double>(Global::getTPS());
        double timeWarp = std::min(static_cast<double>(pl->m_gameState.m_timeWarp), 1.0);
        double timestep = physicsDt * timeWarp;
        if (timestep <= 0.0)
            timestep = physicsDt;

        if (g.frameStepper) {
            if (pl->m_player1 && pl->m_player1->m_isDead) {
                if (g.mod->getSavedValue<bool>("macro_instant_respawn"))
                    pl->resetLevel();
                return;
            }

            g.safeMode = true;
            if (!g.stepFrame) {
                g.schedulerUpdating = true;
                g.schedulerFrozenUpdate = true;
                g.schedulerStepCount = 1;
                CCScheduler::update(0.0f);
                g.schedulerStepCount = 1;
                g.schedulerFrozenUpdate = false;
                g.schedulerUpdating = false;
                return;
            }

            g.stepFrame = false;
            g.schedulerOverflow = 0.0;
            g.schedulerUpdating = true;
            g.schedulerStepCount = 1;
            CCScheduler::update(static_cast<float>(timestep));
            g.schedulerStepCount = 1;
            g.schedulerUpdating = false;
            PracticeFix::saveFrameStepperFrame();
            Global::syncFrameStepperMusic();
            return;
        }

        if (!g.lockDelta) {
            g.schedulerOverflow = 0.0;
            return CCScheduler::update(dt * speedhack);
        }

        g.schedulerOverflow += static_cast<double>(dt) * timeWarp * speedhack;
        int steps = static_cast<int>(std::floor(g.schedulerOverflow / timestep));

        if (steps <= 0)
            return;

        g.schedulerOverflow -= static_cast<double>(steps) * timestep;
        g.schedulerUpdating = true;

        auto runUpdate = [&](int stepCount, double delta) {
            if (stepCount <= 0)
                return;
            g.schedulerStepCount = stepCount;
            CCScheduler::update(static_cast<float>(delta));
            g.schedulerStepCount = 1;
        };

        for (int i = 0; i < steps; i++)
            runUpdate(1, physicsDt);

        g.schedulerUpdating = false;
    }

};

class $modify(PlayerObject) {

    void playDeathEffect() {
        if (!Global::get()
                 .mod
                 ->getSavedValue<bool>(
                     "macro_no_death_effect"
                 )) {
            PlayerObject::playDeathEffect();
        }
    }

    void playSpawnEffect() {
        if (!Global::get()
                 .mod
                 ->getSavedValue<bool>(
                     "macro_no_respawn_flash"
                 )) {
            PlayerObject::playSpawnEffect();
        }
    }
};

class $modify(PlayLayer) {

    struct Fields {
        CCObject* slopeFix = nullptr;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& g = Global::get();

        if (!g.autosaveEnabled)
            return;

        if (!g.autosaveIntervalEnabled)
            return;

        if (g.autosaveCheck <
            g.autosaveInterval) {

            g.autosaveCheck += dt;
            return;
        }

        g.autosaveCheck = 0.f;

        int currentTime =
            asp::time::SystemTime::now()
                .timeSinceEpoch()
                .millis();

        Macro::autoSave(
            m_level,
            currentTime
        );
    }

    void destroyPlayer(
        PlayerObject* player,
        GameObject* object
    ) {
        if (player != m_player1 &&
            player != m_player2) {

            return PlayLayer::destroyPlayer(
                player,
                object
            );
        }

        if (!m_fields->slopeFix)
            m_fields->slopeFix = object;

        auto& g = Global::get();

        bool player2 =
            player == m_player2;

        bool allowDeath =
            (!g.mod->getSavedValue<bool>(
                 "macro_noclip"
             )) ||
            (!g.mod->getSavedValue<bool>(
                 player2
                     ? "macro_noclip_p2"
                     : "macro_noclip_p1"
             )) ||
            (m_fields->slopeFix == object);

        if (allowDeath) {
            PlayLayer::destroyPlayer(
                player,
                object
            );
        } else {
            g.safeMode = true;
        }

        if (getActionByTag(16) &&
            g.mod->getSavedValue<bool>(
                "respawn_time_enabled"
            )) {

            stopActionByTag(16);

            auto* seq =
                CCSequence::create(
                    CCDelayTime::create(
                        g.mod->getSavedValue<double>(
                            "respawn_time"
                        )
                    ),
                    CCCallFunc::create(
                        this,
                        callfunc_selector(
                            PlayLayer::delayedResetLevel
                        )
                    ),
                    nullptr
                );

            seq->setTag(16);

            runAction(seq);
        }
    }

    void showNewBest(
        bool p0,
        int p1,
        int p2,
        bool p3,
        bool p4,
        bool p5
    ) {
        auto& g = Global::get();

        if (!g.safeMode ||
            !Mod::get()->getSavedValue<bool>(
                "macro_auto_safe_mode"
            )) {

            PlayLayer::showNewBest(
                p0,
                p1,
                p2,
                p3,
                p4,
                p5
            );
        }
    }

    void levelComplete() {
        auto& g = Global::get();

        g.firstAttempt = true;

        if (g.state == state::recording &&
            g.autosaveEnabled &&
            g.mod->getSavedValue<bool>(
                "autosave_levelend_enabled"
            )) {

            Macro::autoSave(
                nullptr,
                g.currentSession
            );
        }

        bool wasTestMode =
            m_isTestMode;

        if (g.safeMode &&
            g.mod->getSavedValue<bool>(
                "macro_auto_safe_mode"
            )) {

            m_isTestMode = true;
        }

        if (m_isPracticeMode)
            g.safeMode = false;

        PlayLayer::levelComplete();

        Macro::resetState(true);

        m_isTestMode = wasTestMode;
    }
};
