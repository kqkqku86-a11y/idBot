#include "show_trajectory.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include "../physics/trajectory_physics.hpp"

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

bool ShowTrajectory::isFakePlayer(PlayerObject* player) {
    return player && (player == t.fakePlayer1 || player == t.fakePlayer2);
}

bool ShowTrajectory::hasActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object || object->m_isMultiActivate)
        return false;

    auto key = reinterpret_cast<uintptr_t>(object);
    if (player == t.fakePlayer1)
        return t.activatedObjectsP1.contains(key);
    if (player == t.fakePlayer2)
        return t.activatedObjectsP2.contains(key);

    return false;
}

bool ShowTrajectory::realPlayerHasActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object)
        return false;

    PlayerObject* realPlayer = player;
    auto* pl = PlayLayer::get();
    if (pl && player == t.fakePlayer1)
        realPlayer = pl->m_player1;
    else if (pl && player == t.fakePlayer2)
        realPlayer = pl->m_player2;

    if (!realPlayer)
        return false;

    bool platformerActivated = !realPlayer->m_isPlatformer
        ? object->m_isMultiActivate
        : object->m_isNoMultiActivate;
    if (platformerActivated)
        return false;

    if (!realPlayer->m_isSecondPlayer)
        return object->m_activatedByPlayer1;

    return object->m_activatedByPlayer2;
}

void ShowTrajectory::rememberActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object)
        return;

    if (player == t.fakePlayer1)
        t.activatedObjectsP1.insert({reinterpret_cast<uintptr_t>(object), true});
    else if (player == t.fakePlayer2)
        t.activatedObjectsP2.insert({reinterpret_cast<uintptr_t>(object), true});
}

void ShowTrajectory::snapshotObject(EffectGameObject* object) {
    if (!object)
        return;

    auto key = reinterpret_cast<uintptr_t>(object);
    if (t.snapshotObjects.contains(key))
        return;
    t.snapshotObjects.insert(key);

    auto* ring = typeinfo_cast<RingObject*>(object);
    t.objSnapshot.push_back({
        object,
        ring,
        object->m_isActivated,
        object->m_activatedByPlayer1,
        object->m_activatedByPlayer2,
        ring ? ring->m_claimTouch : false,
        object->m_isDisabled,
        object->m_isDisabled2,
    });
}

void ShowTrajectory::restoreSnapshots() {
    for (auto const& snapshot : t.objSnapshot) {
        if (!snapshot.obj)
            continue;

        snapshot.obj->m_isActivated = snapshot.isActivated;
        snapshot.obj->m_activatedByPlayer1 = snapshot.activatedByP1;
        snapshot.obj->m_activatedByPlayer2 = snapshot.activatedByP2;
        snapshot.obj->m_isDisabled = snapshot.isDisabled;
        snapshot.obj->m_isDisabled2 = snapshot.isDisabled2;

        if (snapshot.ring) {
            snapshot.ring->m_claimTouch = snapshot.claimTouch;
        }
    }

    t.objSnapshot.clear();
    t.snapshotObjects.clear();
}

void ShowTrajectory::hideFakePlayer(PlayerObject* player) {
    if (!player)
        return;

    player->setVisible(false);
    player->setOpacity(0);
    player->m_playEffects = false;
    player->m_maybeReducedEffects = true;
    if (player->getParent())
        player->removeFromParentAndCleanup(false);
}

void ShowTrajectory::releaseFakePlayers() {
    auto releasePlayer = [](PlayerObject*& player) {
        if (!player)
            return;

        hideFakePlayer(player);
        player->release();
        player = nullptr;
    };

    releasePlayer(t.fakePlayer1);
    releasePlayer(t.fakePlayer2);
}

PlayerObject* ShowTrajectory::ensureFakePlayer(PlayLayer* pl, bool player1) {
    if (!pl)
        return nullptr;

    PlayerObject*& fake = player1 ? t.fakePlayer1 : t.fakePlayer2;
    if (!fake) {
        fake = PlayerObject::create(1, 1, pl, pl, true);
        if (!fake)
            return nullptr;

        fake->retain();
        fake->setPosition({ 0, 105 });
        fake->setID(player1 ? "xdbot-trajectory-fake-player1" : "xdbot-trajectory-fake-player2");
    }

    hideFakePlayer(fake);
    return fake;
}

void ShowTrajectory::updateTrajectory(PlayLayer* pl) {
    if (!pl || !pl->m_player1)
        return;

    auto& bot = Bot::get();
    bool platformerBothSides = bot.trajectoryBothSides;

    bot.safeMode = true;

    t.creatingTrajectory = true;
    bot.creatingTrajectory = true;

    t.trajectoryNode()->setVisible(true);
    t.trajectoryNode()->clear();

    auto baseGameState = pl->m_gameState;
    EffectManagerState baseEffectState;
    EffectManagerState* baseEffectStatePtr = nullptr;
    if (pl->m_effectManager) {
        pl->m_effectManager->saveToState(baseEffectState);
        baseEffectStatePtr = &baseEffectState;
    }

    if (ensureFakePlayer(pl, true)) {
        bool simulateBoth = pl->m_gameState.m_isDualMode && !pl->m_levelSettings->m_twoPlayerMode;
        if (simulateBoth && pl->m_player2)
            ensureFakePlayer(pl, false);

        createTrajectory(pl, true, Hold, simulateBoth, baseGameState, baseEffectStatePtr);
        createTrajectory(pl, true, Swift, simulateBoth, baseGameState, baseEffectStatePtr);
        createTrajectory(pl, true, Release, simulateBoth, baseGameState, baseEffectStatePtr);

        if (pl->m_levelSettings->m_platformerMode && platformerBothSides) {
            createTrajectory(pl, true, Hold | Left, simulateBoth, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, true, Swift | Left, simulateBoth, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, true, Release | Left, simulateBoth, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, true, Hold | Right, simulateBoth, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, true, Swift | Right, simulateBoth, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, true, Release | Right, simulateBoth, baseGameState, baseEffectStatePtr);
        }
    }

    if (pl->m_player2 && pl->m_gameState.m_isDualMode && pl->m_levelSettings->m_twoPlayerMode) {
        if (ensureFakePlayer(pl, false)) {
            createTrajectory(pl, false, Hold, false, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, false, Swift, false, baseGameState, baseEffectStatePtr);
            createTrajectory(pl, false, Release, false, baseGameState, baseEffectStatePtr);

            if (pl->m_levelSettings->m_platformerMode && platformerBothSides) {
                createTrajectory(pl, false, Hold | Left, false, baseGameState, baseEffectStatePtr);
                createTrajectory(pl, false, Swift | Left, false, baseGameState, baseEffectStatePtr);
                createTrajectory(pl, false, Release | Left, false, baseGameState, baseEffectStatePtr);
                createTrajectory(pl, false, Hold | Right, false, baseGameState, baseEffectStatePtr);
                createTrajectory(pl, false, Swift | Right, false, baseGameState, baseEffectStatePtr);
                createTrajectory(pl, false, Release | Right, false, baseGameState, baseEffectStatePtr);
            }
        }
    }

    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectStatePtr)
        pl->m_effectManager->loadFromState(*baseEffectStatePtr);

    hideFakePlayer(t.fakePlayer1);
    hideFakePlayer(t.fakePlayer2);
    t.creatingTrajectory = false;
    bot.creatingTrajectory = false;
}
float rot = 0.f;
void ShowTrajectory::applyInitialInput(PlayLayer* pl, PlayerObject* player, PlayerObject* realPlayer, int mode) {
    if (!pl || !player || !realPlayer)
        return;

    switch (mode & (Hold | Swift | Release)) {
    case Hold:
        player->pushButton(PlayerButton::Jump);
        break;
    case Swift:
        player->pushButton(PlayerButton::Jump);
        player->releaseButton(PlayerButton::Jump);
        break;
    case Release:
        player->releaseButton(PlayerButton::Jump);
        player->m_jumpBuffered = false;
        break;
    default:
        break;
    }

    if (!pl->m_levelSettings->m_platformerMode)
        return;

    switch (mode & (Left | Right)) {
    case Left:
        player->releaseButton(PlayerButton::Right);
        player->pushButton(PlayerButton::Left);
        break;
    case Right:
        player->releaseButton(PlayerButton::Left);
        player->pushButton(PlayerButton::Right);
        break;
    default:
        player->releaseButton(PlayerButton::Left);
        player->releaseButton(PlayerButton::Right);
        if (realPlayer->m_isGoingLeft)
            player->pushButton(PlayerButton::Left);
        break;
    }
}

bool ShowTrajectory::iterate(PlayLayer* pl, PlayerObject* player, int mode, cocos2d::ccColor4F color) {
    if (!pl || !player)
        return true;

    CCPoint prevPos = player->getPosition();

    float tps = std::max(Bot::getTPS(), 1.f);
    double physicsSeconds = 1.0 / tps;
    float timeWarp = std::min(pl->m_gameState.m_timeWarp, 1.f);
    if (timeWarp <= 0.f)
        timeWarp = 1.f;

    pl->m_gameState.m_totalTime += physicsSeconds;
    pl->m_gameState.m_unkDouble3 += physicsSeconds / timeWarp;
    pl->m_gameState.m_currentProgress++;
    pl->m_gameState.m_unkUint5 += static_cast<int>(std::round(timeWarp * 1000.f));
    player->m_totalTime += physicsSeconds;

    float playerSpeed = pl->m_gameState.m_timeModRelated;
    if (playerSpeed != 0.f) {
        pl->m_gameState.m_timeModRelated = 0;
        pl->m_gameState.m_timeModRelated2 = 0;
        player->updateTimeMod(playerSpeed, true);
        timeWarp = std::min(pl->m_gameState.m_timeWarp, 1.f);
        if (timeWarp <= 0.f)
            timeWarp = 1.f;
    }

    player->m_collisionLogTop->removeAllObjects();
    player->m_collisionLogBottom->removeAllObjects();
    player->m_collisionLogLeft->removeAllObjects();
    player->m_collisionLogRight->removeAllObjects();

    bool dead = (player == t.fakePlayer1 && t.deadP1) || (player == t.fakePlayer2 && t.deadP2);
    if (dead) {
        drawPlayerHitbox(player, t.trajectoryNode());
        return true;
    }

    t.delta = (1.f / tps) * 60.f * timeWarp;
    player->m_playEffects = false;
    player->update(t.delta);
    player->updateRotation(t.delta);
    player->updatePlayerScale();

    if (pl->checkCollisions(player, t.delta, false) == 1) {
        if (player == t.fakePlayer1)
            t.deadP1 = true;
        else if (player == t.fakePlayer2)
            t.deadP2 = true;
    }

    xdbot::trajectory_physics::checkSpawnObjects(pl, player);

    if (pl->m_effectManager)
        pl->m_effectManager->postCollisionCheck();

    t.trajectoryNode()->drawSegment(prevPos, player->getPosition(), 0.6f, color);

    dead = (player == t.fakePlayer1 && t.deadP1) || (player == t.fakePlayer2 && t.deadP2);
    if (dead) {
        drawPlayerHitbox(player, t.trajectoryNode());
        return true;
    }

    return false;
}

void ShowTrajectory::createTrajectory(
    PlayLayer* pl,
    bool player1,
    int mode,
    bool simulateBothPlayers,
    GJGameState const& baseGameState,
    EffectManagerState* baseEffectState
) {
    if (!pl)
        return;

    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectState)
        pl->m_effectManager->loadFromState(*baseEffectState);

    PlayerObject* fakePlayer = player1 ? t.fakePlayer1 : t.fakePlayer2;
    PlayerObject* realPlayer = player1 ? pl->m_player1 : pl->m_player2;
    PlayerObject* otherFake = player1 ? t.fakePlayer2 : t.fakePlayer1;
    PlayerObject* otherReal = player1 ? pl->m_player2 : pl->m_player1;
    if (!fakePlayer || !realPlayer)
        return;

    PlayerPracticeFixes::transfer(realPlayer, fakePlayer, true);
    hideFakePlayer(fakePlayer);

    if (simulateBothPlayers && otherFake && otherReal) {
        PlayerPracticeFixes::transfer(otherReal, otherFake, true);
        hideFakePlayer(otherFake);
    }

    t.cancelTrajectory = false;
    t.deadP1 = false;
    t.deadP2 = false;
    t.activatedObjectsP1.clear();
    t.activatedObjectsP2.clear();
    t.objSnapshot.clear();
    t.snapshotObjects.clear();

    applyInitialInput(pl, fakePlayer, realPlayer, mode);
    if (simulateBothPlayers && otherFake && otherReal)
        applyInitialInput(pl, otherFake, otherReal, mode);

    cocos2d::ccColor4F color = (mode & Hold) ? t.color1 : (mode & Swift) ? t.color3 : t.color2;
    cocos2d::ccColor4F otherColor = ccc4f(1.f - color.r, 1.f - color.g, 1.f - color.b, color.a);

    int predictionLength = std::max(t.length, 0);
    for (int i = 0; i < predictionLength; i++) {
        if (i >= predictionLength - 40)
            color.a = (predictionLength - i) / 40.f;

        bool stopMain = iterate(pl, fakePlayer, mode, color);
        bool stopOther = false;
        if (simulateBothPlayers && otherFake)
            stopOther = iterate(pl, otherFake, mode, otherColor);

        if (stopMain && (!simulateBothPlayers || stopOther))
            break;
    }

    restoreSnapshots();
    t.activatedObjectsP1.clear();
    t.activatedObjectsP2.clear();
    t.snapshotObjects.clear();
    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectState)
        pl->m_effectManager->loadFromState(*baseEffectState);
    hideFakePlayer(fakePlayer);
    if (simulateBothPlayers)
        hideFakePlayer(otherFake);

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

        float xNew = center.x + (x * cos(angle)) - (y * sin(angle));
        float yNew = center.y + (x * sin(angle)) + (y * cos(angle));

        vertex.x = xNew;
        vertex.y = yNew;
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
    switch (id) {
    case 101:
        player->togglePlayerScale(true, true);
        player->updatePlayerScale();
        break;
    case 99:
        player->togglePlayerScale(false, true);
        player->updatePlayerScale();
        break;
        // case 11:
            // player->flipGravity(true, true); break;
        // case 10:
            // player->flipGravity(false, true); break;
    case 200: player->m_playerSpeed = 0.7f; break;
    case 201: player->m_playerSpeed = 0.9f; break;
    case 202: player->m_playerSpeed = 1.1f; break;
    case 203: player->m_playerSpeed = 1.3f; break;
    case 1334: player->m_playerSpeed = 1.6f; break;
    }
}

cocos2d::CCDrawNode* ShowTrajectory::trajectoryNode() {

    static TrajectoryNode* instance = nullptr;

    if (!instance) {
        instance = TrajectoryNode::create();
        instance->retain();

        cocos2d::_ccBlendFunc  blendFunc;
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

        if (Bot::get().showTrajectory) {
            ShowTrajectory::updateTrajectory(this);
        }

    }

    void playGravityEffect(bool flip) {
        if (!t.creatingTrajectory)
            PlayLayer::playGravityEffect(flip);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        m_objectLayer->addChild(t.trajectoryNode(), 500);
        t.trajectoryNode()->setVisible(false);
    }

    void destroyPlayer(PlayerObject * player, GameObject * gameObject) {
        if (t.creatingTrajectory || (player == t.fakePlayer1 || player == t.fakePlayer2)) {
            t.deathRotation = player->getRotation();
            if (player == t.fakePlayer1)
                t.deadP1 = true;
            else if (player == t.fakePlayer2)
                t.deadP2 = true;
            t.cancelTrajectory = true;
            return;
        }

        PlayLayer::destroyPlayer(player, gameObject);
    }

    void onQuit() {
        if (t.trajectoryNode())
            t.trajectoryNode()->clear();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (!t.creatingTrajectory)
            PlayLayer::playEndAnimationToPos(p0);
    }

};

class $modify(PauseLayer) {
    void goEdit() {
        if (t.trajectoryNode())
            t.trajectoryNode()->clear();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        PauseLayer::goEdit();
    }
};

class $modify(GJBaseGameLayer) {

    void collisionCheckObjects(PlayerObject * p0, gd::vector<GameObject*>*objects, int p2, float p3) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            xdbot::trajectory_physics::collisionCheckObjects(this, p0, objects, p2, p3);
            return;
        }

        GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);
    }

    bool canBeActivatedByPlayer(PlayerObject * p0, EffectGameObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            if (ShowTrajectory::hasActivated(p0, p1) ||
                ShowTrajectory::realPlayerHasActivated(p0, p1))
                return false;

            ShowTrajectory::snapshotObject(p1);
            return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
        }

        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void playerTouchedRing(PlayerObject * p0, RingObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            ShowTrajectory::snapshotObject(p1);
            ShowTrajectory::rememberActivated(p0, p1);
            xdbot::trajectory_physics::ringJump(p0, p1);
        } else if (!t.creatingTrajectory)
            GJBaseGameLayer::playerTouchedRing(p0, p1);
    }

    void playerTouchedTrigger(PlayerObject * p0, EffectGameObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            xdbot::trajectory_physics::triggerObject(p1, this, p0);
        } else if (!t.creatingTrajectory)
            GJBaseGameLayer::playerTouchedTrigger(p0, p1);
    }

    void activateSFXTrigger(SFXTriggerGameObject * p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSFXTrigger(p0);

    }
    void activateSongEditTrigger(SongTriggerGameObject * p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSongEditTrigger(p0);

    }
    // void activateSongTrigger(SongTriggerGameObject * p0) {
    //     if (!t.creatingTrajectory)
    //         GJBaseGameLayer::activateSongTrigger(p0);
    // }

    void gameEventTriggered(GJGameEvent p0, int p1, int p2) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::gameEventTriggered(p0, p1, p2);
    }

    void teleportPlayer(TeleportPortalObject* object, PlayerObject* player) {
        if (ShowTrajectory::isFakePlayer(player)) {
            xdbot::trajectory_physics::teleportPlayer(this, object, player);
        } else {
            GJBaseGameLayer::teleportPlayer(object, player);
        }
    }

    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(player)) {
            xdbot::trajectory_physics::flipGravity(player, flip);
        } else {
            GJBaseGameLayer::flipGravity(player, flip, noEffects);
        }
    }

};

class $modify(PlayerObject) {

    void update(float dt) {
        PlayerObject::update(dt);
        t.delta = dt;
    }

    void playSpiderDashEffect(cocos2d::CCPoint p0, cocos2d::CCPoint p1) {
        if (!t.creatingTrajectory)
            PlayerObject::playSpiderDashEffect(p0, p1);
    }

    void incrementJumps() {
        if (ShowTrajectory::isFakePlayer(this))
            return;
        if (!t.creatingTrajectory)
            PlayerObject::incrementJumps();
    }

    void ringJump(RingObject * p0, bool p1) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::ringJump(this, p0);
        } else if (!t.creatingTrajectory)
            PlayerObject::ringJump(p0, p1);
    }

    void bumpPlayer(float force, int objectType, bool noEffects, GameObject* object) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::bumpPlayer(this, force, objectType, noEffects, object);
        } else {
            PlayerObject::bumpPlayer(force, objectType, noEffects, object);
        }
    }

    void propellPlayer(float force, bool noEffects, int objectType) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::propellPlayer(this, force, noEffects, objectType);
        } else {
            PlayerObject::propellPlayer(force, noEffects, objectType);
        }
    }

    void startDashing(DashRingObject* object) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::startDashing(this, object);
        } else {
            PlayerObject::startDashing(object);
        }
    }

    void togglePlayerScale(bool enable, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::togglePlayerScale(this, enable);
        } else {
            PlayerObject::togglePlayerScale(enable, noEffects);
        }
    }

    void flipGravity(bool flip, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::flipGravity(this, flip);
        } else {
            PlayerObject::flipGravity(flip, noEffects);
        }
    }

    void stopDashing() {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::stopDashing(this);
        } else {
            PlayerObject::stopDashing();
        }
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

    void triggerObject(GJBaseGameLayer * p0, int p1, const gd::vector<int>*p2) {
        if (!t.creatingTrajectory)
            EffectGameObject::triggerObject(p0, p1, p2);
    }
};
