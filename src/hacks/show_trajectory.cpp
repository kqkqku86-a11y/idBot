#include "show_trajectory.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/modify/EnhancedGameObject.hpp>

ShowTrajectory& t = ShowTrajectory::get();

$execute {
    t.color1 = ccc4FFromccc3B(Mod::get()->getSavedValue<cocos2d::ccColor3B>("trajectory_color1"));
    t.color2 = ccc4FFromccc3B(Mod::get()->getSavedValue<cocos2d::ccColor3B>("trajectory_color2"));
    t.length = geode::utils::numFromString<int>(Mod::get()->getSavedValue<std::string>("trajectory_length")).unwrapOr(0);
    t.updateMergedColor();
};

void ShowTrajectory::trajectoryOff() {
    if (t.trajectoryNode()) {
        t.trajectoryNode()->clear();
        t.trajectoryNode()->setVisible(false);
    }
}

static void applyGamemode(PlayerObject* fake, PlayerObject* real) {
    // Determine target mode from real player
    const bool targetDart   = real->m_isDart;
    const bool targetBall   = real->m_isBall;
    const bool targetShip   = real->m_isShip;
    const bool targetBird   = real->m_isBird;
    const bool targetRobot  = real->m_isRobot;
    const bool targetSpider = real->m_isSpider;
    const bool targetSwing  = real->m_isSwing;

    fake->m_isUpsideDown = real->m_isUpsideDown;
    fake->m_isSideways   = real->m_isSideways;

    // Disable any currently active mode on fake player first
    // This runs the cleanup path (resets width/height/frames/etc.)
    if (fake->m_isDart   && !targetDart)   { fake->toggleDartMode(false, true);   }
    if (fake->m_isBall   && !targetBall)   { fake->toggleRollMode(false, true);   }
    if (fake->m_isShip   && !targetShip)   { fake->toggleFlyMode(false, true);    }
    if (fake->m_isBird   && !targetBird)   { fake->toggleBirdMode(false, true);   }
    if (fake->m_isRobot  && !targetRobot)  { fake->toggleRobotMode(false, true);  }
    if (fake->m_isSpider && !targetSpider) { fake->toggleSpiderMode(false, true); }
    if (fake->m_isSwing  && !targetSwing)  { fake->toggleSwingMode(false, true);  }

    // Now enable target mode (early-exit won't fire since flags were just cleared)
    if (targetDart)   { fake->toggleDartMode(true, true);   return; }
    if (targetBall)   { fake->toggleRollMode(true, true);   return; }
    if (targetShip)   { fake->toggleFlyMode(true, true);    return; }
    if (targetBird)   { fake->toggleBirdMode(true, true);   return; }
    if (targetRobot)  { fake->toggleRobotMode(true, true);  return; }
    if (targetSpider) { fake->toggleSpiderMode(true, true); return; }
    if (targetSwing)  { fake->toggleSwingMode(true, true);  return; }

    // Cube — restore hitbox dimensions that wave/spider may have changed
    fake->m_unkAngle1 = 20.0;  // cube default
    fake->m_width  = (fake->m_vehicleSize == 1.0f) ? 20.0f : 12.0f;
    fake->m_height = (fake->m_vehicleSize == 1.0f) ? 20.0f : 12.0f;
    fake->updatePlayerScale();
}

void ShowTrajectory::updateTrajectory(PlayLayer* pl) {
    if (!t.fakePlayer1 || !t.fakePlayer2) return;

    auto& g = Global::get();
    g.safeMode = true;
    t.creatingTrajectory = true;
    g.creatingTrajectory = true;

    t.trajectoryNode()->setVisible(true);
    t.trajectoryNode()->clear();

    if (pl->m_player1) {
        createTrajectory(pl, t.fakePlayer1, pl->m_player1, true);
        createTrajectory(pl, t.fakePlayer2, pl->m_player1, false);

        if (g.trajectoryBothSides) {
            createTrajectory(pl, t.fakePlayer1, pl->m_player1, true, true);
            createTrajectory(pl, t.fakePlayer2, pl->m_player1, false, true);
        }
    }

    if (pl->m_gameState.m_isDualMode && pl->m_player2) {
        createTrajectory(pl, t.fakePlayer2, pl->m_player2, true);
        createTrajectory(pl, t.fakePlayer1, pl->m_player2, false);

        if (g.trajectoryBothSides) {
            createTrajectory(pl, t.fakePlayer2, pl->m_player2, true, true);
            createTrajectory(pl, t.fakePlayer1, pl->m_player2, false, true);
        }
    }

    t.creatingTrajectory = false;
    g.creatingTrajectory = false;
}

void ShowTrajectory::createTrajectory(PlayLayer* pl, PlayerObject* fakePlayer, PlayerObject* realPlayer, bool hold, bool inverted) {
    bool player2 = pl->m_player2 == realPlayer;

    GJGameState savedState = pl->m_gameState;

    t.objSnapshot.clear();
    float baseX = realPlayer->getPositionX();
    float maxAhead = t.length * 15.f;
    if (pl->m_objects) {
        for (unsigned i = 0; i < pl->m_objects->count(); i++) {
            auto* ego = typeinfo_cast<EffectGameObject*>(pl->m_objects->objectAtIndex(i));
            if (!ego) continue;
            float ox = ego->getPositionX();
            if (ox < baseX - 200.f || ox > baseX + maxAhead) continue;
            auto* ring = typeinfo_cast<RingObject*>(ego);
            t.objSnapshot.push_back({
                ego, ring,
                ego->m_isActivated,
                ego->m_activated,
                ego->m_activatedByPlayer1,
                ego->m_activatedByPlayer2,
                ring ? ring->m_claimTouch : false
            });
        }
    }

    auto restoreObjs = [&]() {
        for (auto& s : t.objSnapshot) {
            s.obj->m_isActivated        = s.isActivated;
            s.obj->m_activated          = s.activated;
            s.obj->m_activatedByPlayer1 = s.activatedByP1;
            s.obj->m_activatedByPlayer2 = s.activatedByP2;
            if (s.ring) s.ring->m_claimTouch = s.claimTouch;
        }
    };

    PlayerObject** slot = (player2) ? &pl->m_player2 : &pl->m_player1;
    *slot = fakePlayer;

    const bool prevDart   = fakePlayer->m_isDart;
    const bool prevBall   = fakePlayer->m_isBall;
    const bool prevShip   = fakePlayer->m_isShip;
    const bool prevBird   = fakePlayer->m_isBird;
    const bool prevRobot  = fakePlayer->m_isRobot;
    const bool prevSpider = fakePlayer->m_isSpider;
    const bool prevSwing  = fakePlayer->m_isSwing;

    PlayerPracticeFixes::transfer(realPlayer, fakePlayer, true);

    fakePlayer->m_isUpsideDown = realPlayer->m_isUpsideDown;
    fakePlayer->m_isSideways   = realPlayer->m_isSideways;
    fakePlayer->m_vehicleSize  = realPlayer->m_vehicleSize;

    const bool targetDart   = realPlayer->m_isDart;
    const bool targetBall   = realPlayer->m_isBall;
    const bool targetShip   = realPlayer->m_isShip;
    const bool targetBird   = realPlayer->m_isBird;
    const bool targetRobot  = realPlayer->m_isRobot;
    const bool targetSpider = realPlayer->m_isSpider;
    const bool targetSwing  = realPlayer->m_isSwing;

    fakePlayer->m_isDart   = prevDart;
    fakePlayer->m_isBall   = prevBall;
    fakePlayer->m_isShip   = prevShip;
    fakePlayer->m_isBird   = prevBird;
    fakePlayer->m_isRobot  = prevRobot;
    fakePlayer->m_isSpider = prevSpider;
    fakePlayer->m_isSwing  = prevSwing;

    if (prevDart   && !targetDart)   { fakePlayer->toggleDartMode(false, true);   fakePlayer->resetPlayerIcon(); }
    if (prevBall   && !targetBall)   { fakePlayer->toggleRollMode(false, true);   }
    if (prevShip   && !targetShip)   { fakePlayer->toggleFlyMode(false, true);    fakePlayer->resetPlayerIcon(); }
    if (prevBird   && !targetBird)   { fakePlayer->toggleBirdMode(false, true);   fakePlayer->resetPlayerIcon(); }
    if (prevRobot  && !targetRobot)  { fakePlayer->toggleRobotMode(false, true);  }
    if (prevSpider && !targetSpider) { fakePlayer->toggleSpiderMode(false, true); fakePlayer->resetPlayerIcon(); }
    if (prevSwing  && !targetSwing)  { fakePlayer->toggleSwingMode(false, true);  fakePlayer->resetPlayerIcon(); }

    if (targetDart)        { fakePlayer->toggleDartMode(true, true);   }
    else if (targetBall)   { fakePlayer->toggleRollMode(true, true);   }
    else if (targetShip)   { fakePlayer->toggleFlyMode(true, true);    }
    else if (targetBird)   { fakePlayer->toggleBirdMode(true, true);   }
    else if (targetRobot)  { fakePlayer->toggleRobotMode(true, true);  }
    else if (targetSpider) { fakePlayer->toggleSpiderMode(true, true); }
    else if (targetSwing)  { fakePlayer->toggleSwingMode(true, true);  }
    else {
        fakePlayer->m_isDart   = false;
        fakePlayer->m_isBall   = false;
        fakePlayer->m_isShip   = false;
        fakePlayer->m_isBird   = false;
        fakePlayer->m_isRobot  = false;
        fakePlayer->m_isSpider = false;
        fakePlayer->m_isSwing  = false;
        fakePlayer->resetPlayerIcon();
    }

    fakePlayer->m_collisionLogTop->removeAllObjects();
    fakePlayer->m_collisionLogBottom->removeAllObjects();
    fakePlayer->m_collisionLogLeft->removeAllObjects();
    fakePlayer->m_collisionLogRight->removeAllObjects();

    fakePlayer->m_ringRelatedSet.clear();
    fakePlayer->m_touchedRings.clear();
    fakePlayer->m_touchedRing = false;
    fakePlayer->m_touchedCustomRing = false;
    fakePlayer->m_touchedGravityPortal = false;
    fakePlayer->m_ringJumpRelated = false;
    fakePlayer->m_padRingRelated = false;
    if (fakePlayer->m_touchingRings) fakePlayer->m_touchingRings->removeAllObjects();

    fakePlayer->setVisible(false);
    t.cancelTrajectory = false;

    hold ? fakePlayer->pushButton(PlayerButton::Jump) : fakePlayer->releaseButton(PlayerButton::Jump);
    if (pl->m_isPlatformer)
        (inverted ? !realPlayer->m_isGoingLeft : realPlayer->m_isGoingLeft)
            ? fakePlayer->pushButton(PlayerButton::Left)
            : fakePlayer->pushButton(PlayerButton::Right);

    for (int i = 0; i < t.length; i++) {
        CCPoint prevPos = fakePlayer->getPosition();

        if (hold) {
            if (player2)
                t.player2Trajectory[i] = prevPos;
            else
                t.player1Trajectory[i] = prevPos;
        }

        fakePlayer->m_collisionLogTop->removeAllObjects();
        fakePlayer->m_collisionLogBottom->removeAllObjects();
        fakePlayer->m_collisionLogLeft->removeAllObjects();
        fakePlayer->m_collisionLogRight->removeAllObjects();

        pl->checkCollisions(fakePlayer, t.delta, false);

        if (t.cancelTrajectory) {
            fakePlayer->updatePlayerScale();
            drawPlayerHitbox(fakePlayer, t.trajectoryNode());
            break;
        }

        fakePlayer->update(t.delta);
        fakePlayer->updateRotation(t.delta);
        fakePlayer->updatePlayerScale();

        cocos2d::ccColor4F color = hold ? t.color1 : t.color2;

        if (!hold) {
            if ((player2 && t.player2Trajectory[i] == prevPos) || (!player2 && t.player1Trajectory[i] == prevPos))
                color = t.color3;
        }

        if (i >= t.length - 40)
            color.a = (t.length - i) / 40.f;

        t.trajectoryNode()->drawSegment(prevPos, fakePlayer->getPosition(), 0.6f, color);
    }

    *slot = realPlayer;

    pl->m_gameState = savedState;
    restoreObjs();
}

void ShowTrajectory::drawPlayerHitbox(PlayerObject* player, CCDrawNode* drawNode) {
    cocos2d::CCRect bigRect = player->GameObject::getObjectRect();
    cocos2d::CCRect smallRect = player->GameObject::getObjectRect(0.3, 0.3);

    std::vector<cocos2d::CCPoint> vertices = ShowTrajectory::getVertices(player, bigRect, t.deathRotation);
    drawNode->drawPolygon(&vertices[0], 4, ccc4f(t.color2.r, t.color2.g, t.color2.b, 0.2f), 0.5, t.color2);

    vertices = ShowTrajectory::getVertices(player, smallRect, t.deathRotation);
    drawNode->drawPolygon(&vertices[0], 4, ccc4f(t.color3.r, t.color3.g, t.color3.b, 0.2f), 0.35, ccc4f(t.color3.r, t.color3.g, t.color3.b, 0.55f));
}

std::vector<cocos2d::CCPoint> ShowTrajectory::getVertices(PlayerObject* player, cocos2d::CCRect rect, float rotation) {
    std::vector<cocos2d::CCPoint> vertices = {
        ccp(rect.getMinX(), rect.getMaxY()),
        ccp(rect.getMaxX(), rect.getMaxY()),
        ccp(rect.getMaxX(), rect.getMinY()),
        ccp(rect.getMinX(), rect.getMinY())
    };

    cocos2d::CCPoint center = ccp(
        (rect.getMinX() + rect.getMaxX()) / 2.f,
        (rect.getMinY() + rect.getMaxY()) / 2.f
    );

    float size = static_cast<int>(rect.getMaxX() - rect.getMinX());

    if ((size == 18 || size == 5) && player->getScale() == 1) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) / 0.6f;
            vertex.y = center.y + (vertex.y - center.y) / 0.6f;
        }
    }

    if ((size == 7 || size == 30 || size == 29 || size == 9) && player->getScale() != 1) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) * 0.6;
            vertex.y = center.y + (vertex.y - center.y) * 0.6f;
        }
    }

    if (player->m_isDart) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) * 0.3f;
            vertex.y = center.y + (vertex.y - center.y) * 0.3f;
        }
    }

    float angle = CC_DEGREES_TO_RADIANS(rotation * -1.f);
    for (auto& vertex : vertices) {
        float x = vertex.x - center.x;
        float y = vertex.y - center.y;
        vertex.x = center.x + (x * cos(angle)) - (y * sin(angle));
        vertex.y = center.y + (x * sin(angle)) + (y * cos(angle));
    }

    return vertices;
}

void ShowTrajectory::updateMergedColor() {
    cocos2d::ccColor4F newColor = { 0.f, 0.f, 0.f, 1.f };
    newColor.r = (color1.r + color2.r) / 2;
    newColor.b = (color1.b + color2.b) / 2;
    newColor.g = (color1.g + color2.g) / 2;
    newColor.r = std::min(1.f, newColor.r + 0.45f);
    newColor.g = std::min(1.f, newColor.g + 0.45f);
    newColor.b = std::min(1.f, newColor.b + 0.45f);
    color3 = newColor;
}

void ShowTrajectory::handlePortal(PlayerObject* player, int id) {
    if (!portalIDs.contains(id)) return;
    switch (id) {
    case 101:
        player->togglePlayerScale(true, true);
        player->updatePlayerScale();
        break;
    case 99:
        player->togglePlayerScale(false, true);
        player->updatePlayerScale();
        break;
    }
}

cocos2d::CCDrawNode* ShowTrajectory::trajectoryNode() {
    static TrajectoryNode* instance = nullptr;
    if (!instance) {
        instance = TrajectoryNode::create();
        instance->retain();
        cocos2d::_ccBlendFunc blendFunc;
        blendFunc.src = GL_SRC_ALPHA;
        blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
        instance->setBlendFunc(blendFunc);
    }
    return instance;
}

class $modify(PlayLayer) {

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!t.trajectoryNode() || t.creatingTrajectory) return;
        if (Global::get().showTrajectory)
            ShowTrajectory::updateTrajectory(this);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        t.fakePlayer1 = nullptr;
        t.fakePlayer2 = nullptr;
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;

        t.fakePlayer1 = PlayerObject::create(1, 1, this, this, true);
        t.fakePlayer1->retain();
        t.fakePlayer1->setPosition({ 0, 105 });
        t.fakePlayer1->setVisible(false);
        m_objectLayer->addChild(t.fakePlayer1);

        t.fakePlayer2 = PlayerObject::create(1, 1, this, this, true);
        t.fakePlayer2->retain();
        t.fakePlayer2->setPosition({ 0, 105 });
        t.fakePlayer2->setVisible(false);
        m_objectLayer->addChild(t.fakePlayer2);

        m_objectLayer->addChild(t.trajectoryNode(), 500);
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (player == t.fakePlayer1 || player == t.fakePlayer2) {
            t.deathRotation = player->getRotation();
            t.cancelTrajectory = true;
            return;
        }
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void onQuit() {
        if (t.trajectoryNode()) t.trajectoryNode()->clear();
        t.fakePlayer1 = nullptr;
        t.fakePlayer2 = nullptr;
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (!t.creatingTrajectory)
            PlayLayer::playEndAnimationToPos(p0);
    }
};

class $modify(PauseLayer) {
    void goEdit() {
        if (t.trajectoryNode()) t.trajectoryNode()->clear();
        t.fakePlayer1 = nullptr;
        t.fakePlayer2 = nullptr;
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        PauseLayer::goEdit();
    }
};

class $modify(GJBaseGameLayer) {

    void collisionCheckObjects(PlayerObject* p0, gd::vector<GameObject*>* objects, int p2, float p3) {
        if (t.creatingTrajectory) {
            std::vector<GameObject*> disabledObjects;
            for (const auto& obj : *objects) {
                if (!obj) continue;
                if ((!objectTypes.contains(static_cast<int>(obj->m_objectType)) && !portalIDs.contains(obj->m_objectID)) || collectibleIDs.contains(obj->m_objectID)) {
                    if (obj->m_isDisabled || obj->m_isDisabled2) continue;
                    disabledObjects.push_back(obj);
                    obj->m_isDisabled = true;
                    obj->m_isDisabled2 = true;
                }
            }
            GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);
            for (const auto& obj : disabledObjects) {
                if (!obj) continue;
                obj->m_isDisabled = false;
                obj->m_isDisabled2 = false;
            }
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (t.creatingTrajectory) {
            if (p0 == t.fakePlayer1 || p0 == t.fakePlayer2) {
                if (portalIDs.contains(p1->m_objectID)) {
                    ShowTrajectory::handlePortal(p0, p1->m_objectID);
                    return false;
                }
                return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
            }
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void playerTouchedRing(PlayerObject* p0, RingObject* p1) {
        if (t.creatingTrajectory) {
            if (p0 == t.fakePlayer1 || p0 == t.fakePlayer2)
                GJBaseGameLayer::playerTouchedRing(p0, p1);
            return;
        }
        GJBaseGameLayer::playerTouchedRing(p0, p1);
    }
    
    void playerTouchedTrigger(PlayerObject* p0, EffectGameObject* p1) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::playerTouchedTrigger(p0, p1);
    }

    void activateSFXTrigger(SFXTriggerGameObject* p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSFXTrigger(p0);
    }

    void activateSongEditTrigger(SongTriggerGameObject* p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSongEditTrigger(p0);
    }

    void gameEventTriggered(GJGameEvent p0, int p1, int p2) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::gameEventTriggered(p0, p1, p2);
    }
};

class $modify(PlayerObject) {

    void update(float dt) {
        PlayerObject::update(dt);
        t.delta = dt;
    }
    
    void toggleBirdMode(bool enable, bool noEffects) {
        PlayerObject::toggleBirdMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleFlyMode(bool enable, bool noEffects) {
        PlayerObject::toggleFlyMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleRollMode(bool enable, bool noEffects) {
        PlayerObject::toggleRollMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleDartMode(bool enable, bool noEffects) {
        PlayerObject::toggleDartMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleRobotMode(bool enable, bool noEffects) {
        PlayerObject::toggleRobotMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleSpiderMode(bool enable, bool noEffects) {
        PlayerObject::toggleSpiderMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    void toggleSwingMode(bool enable, bool noEffects) {
        PlayerObject::toggleSwingMode(enable, t.creatingTrajectory ? true : noEffects);
    }
    
    void playSpiderDashEffect(cocos2d::CCPoint p0, cocos2d::CCPoint p1) {
        if (!t.creatingTrajectory)
            PlayerObject::playSpiderDashEffect(p0, p1);
    }

    void incrementJumps() {
        if (!t.creatingTrajectory)
            PlayerObject::incrementJumps();
    }

    void ringJump(RingObject* p0, bool p1) {
        if (t.creatingTrajectory) {
            if (this == t.fakePlayer1 || this == t.fakePlayer2)
                PlayerObject::ringJump(p0, p1);
            return;
        }
        PlayerObject::ringJump(p0, p1);
    }
};

class $modify(EnhancedGameObject) {
    bool hasBeenActivated() {
        if (t.creatingTrajectory) return false;
        return EnhancedGameObject::hasBeenActivated();
    }
    bool hasBeenActivatedByPlayer(PlayerObject* player) {
        if (t.creatingTrajectory) return false;
        return EnhancedGameObject::hasBeenActivatedByPlayer(player);
    }
};

class $modify(RingObject) {
    void triggerActivated(float xPosition) {
        if (t.creatingTrajectory) return;
        RingObject::triggerActivated(xPosition);
    }
    void activateObject() {
        if (t.creatingTrajectory) return;
        RingObject::activateObject();
    }
    bool hasBeenActivated() {
        if (t.creatingTrajectory) return false;
        return RingObject::hasBeenActivated();
    }
    void powerOnObject(int state) {
        if (t.creatingTrajectory) return;
        RingObject::powerOnObject(state);
    }
    void spawnCircle() {
        if (t.creatingTrajectory) return;
        RingObject::spawnCircle();
    }
};

class $modify(HardStreak) {
    void addPoint(cocos2d::CCPoint p0) {
        if (!t.creatingTrajectory)
            HardStreak::addPoint(p0);
    }
};

class $modify(GameObject) {
    void playShineEffect() {
        if (!t.creatingTrajectory)
            GameObject::playShineEffect();
    }
};

class $modify(EffectGameObject) {
    void triggerObject(GJBaseGameLayer* p0, int p1, const gd::vector<int>* p2) {
        if (!t.creatingTrajectory)
            EffectGameObject::triggerObject(p0, p1, p2);
    }
};