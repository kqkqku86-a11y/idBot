// practice_fixes.hpp
#pragma once
#include "../includes.hpp"
#include "../macro.hpp"
#include "checkpoint.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class PracticeFix {
    public:
    static bool shouldEnable() {
        PlayLayer* pl = PlayLayer::get();
        if (!pl || !pl->m_isPracticeMode) return false;
        
        auto& g = Global::get();
        if (g.state != state::none) return true;
        
        if (g.alwaysPracticeFixes && pl->m_isPlatformer) return true;
        
        return false;
    }
};

class PlayerPracticeFixes {
    public:
    struct SavedState {
        SupplementalPlayerState supplemental;
    };
    
    static SavedState saveData(PlayerObject* p) {
        SavedState s;
        if (!p) return s;
        s.supplemental = SupplementalPlayerState(p);
        return s;
    }
    
    static void applyData(PlayerObject* p, const SavedState& s) {
        if (!p) return;
        s.supplemental.apply(p);
    }
    
    static void transfer(PlayerObject* from, PlayerObject* to, bool applyPos) {
        if (!from || !to) return;
        SavedState s = saveData(from);
        if (applyPos) {
            to->setPosition(from->getPosition());
            to->setRotation(from->getRotation());
        }
        applyData(to, s);
    }
};