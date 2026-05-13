#include "trajectory_physics.hpp"

#include "../hacks/show_trajectory.hpp"

using namespace geode::prelude;

namespace xdbot::trajectory_physics {
namespace {
float gravitySign(PlayerObject* player) {
    return player && player->m_isUpsideDown ? -1.f : 1.f;
}

void clearCollisionState(PlayerObject* player) {
    if (!player)
        return;

    player->m_collidedBottomMaxY = 0.0;
    player->m_collidedTopMinY = 0.0;
    player->m_collisionLogTop->removeAllObjects();
    player->m_collisionLogBottom->removeAllObjects();
    player->m_collisionLogLeft->removeAllObjects();
    player->m_collisionLogRight->removeAllObjects();
    player->m_lastCollisionBottom = -1;
    player->m_lastCollisionTop = -1;
    player->m_lastCollisionLeft = -1;
    player->m_lastCollisionRight = -1;
}

CCPoint pointForAngle(float angle) {
#ifdef GEODE_IS_WINDOWS
    return ccpForAngle(angle);
#else
    return {std::cos(angle), std::sin(angle)};
#endif
}

cocos2d::CCArray* getGroup(GJBaseGameLayer* layer, int groupID) {
    if (!layer)
        return nullptr;

    groupID = std::clamp(groupID, 0, 9999);
    cocos2d::CCArray* group = layer->m_groups.at(groupID);
    if (!group) {
        group = cocos2d::CCArray::create();
        layer->m_groupDict->setObject(group, groupID);
        layer->m_groups.at(groupID) = group;
    }

    return group;
}

float redirectPlayerForce(PlayerObject* player, float force, float, float, float) {
    if (!player)
        return 0.f;

    CCPoint velocity = {
        static_cast<float>(player->m_platformerXVelocity),
        static_cast<float>(player->m_yVelocity),
    };

    float angle = force * 0.017453292f - std::atan2(velocity.y, velocity.x);
    if (angle != 0.f) {
        float s = std::sin(angle);
        float c = std::cos(angle);
        velocity = CCPoint{
            velocity.x * s - velocity.y * c,
            velocity.x * c + velocity.y * s,
        };
    }

    if (player->m_isSideways)
        std::swap(velocity.x, velocity.y);

    player->m_yVelocity = velocity.y;
    player->m_isAccelerating = true;
    if (player->m_isPlatformer) {
        player->m_platformerXVelocity = velocity.x;
        player->m_affectedByForces = true;
    }

    return player->m_yVelocity;
}
}

void flipGravity(PlayerObject* player, bool gravity) {
    if (!player || player->m_isUpsideDown == gravity)
        return;

    player->m_isUpsideDown = gravity;
    player->m_lastFlipTime = player->m_totalTime;
    player->m_unkA29 = false;
    if (player->m_wasOnSlope || player->m_isOnSlope)
        player->m_slopeFlipGravityRelated = !player->m_slopeFlipGravityRelated;

    clearCollisionState(player);
    player->m_yVelocity *= 0.5;
    player->m_isOnGround = false;

    if (player->m_isBall) {
        player->m_isRotating = false;
        player->m_isBallRotating = false;
        player->m_isBallRotating2 = false;
        player->m_rotationSpeed = 0.0;
        player->runBallRotation2();
    }
}

void propellPlayer(PlayerObject* player, float force, bool, int) {
    if (!player)
        return;

    player->m_maybeIsBoosted = true;
    player->m_isOnGround2 = false;
    player->m_isOnGround = false;
    player->m_isOnSlope = false;
    player->m_wasOnSlope = false;

    float sizeGravity = player->m_vehicleSize != 1.f ? 0.8f : 1.f;
    player->setYVelocity(force * sizeGravity * gravitySign(player) * 16.f, 0);

    if (player->m_isBall || player->m_isSpider || player->m_isSwing)
        player->m_yVelocity *= 0.6;

    if (!player->m_isLocked && !player->m_isDashing) {
        player->m_isRotating = false;
        player->m_isBallRotating = false;
        player->m_isBallRotating2 = false;
        player->m_rotationSpeed = 0.0;
        if (!player->m_isBall)
            player->runBallRotation(player->m_yVelocity);
        else
            player->runNormalRotation(0, 1.0);
    }

    player->m_lastGroundedPos = player->getPosition();
}

void bumpPlayer(PlayerObject* player, float force, int objectType, bool noEffects, GameObject* object) {
    if (!player)
        return;

    if (player->m_isPlatformer || !player->m_fixRobotJump)
        player->m_touchedPad = true;

    if (objectType != static_cast<int>(GameObjectType::SpiderPad)) {
        propellPlayer(player, force, noEffects, objectType);
        player->m_isAccelerating = objectType == static_cast<int>(GameObjectType::RedJumpPad);
        if (player->m_isAccelerating)
            player->m_lastGroundedPos = CCPoint{0.f, 0.f};
        return;
    }

    if (object) {
        bool facing = player->m_isSideways ? object->isFacingLeft() : object->isFacingDown();
        if (player->m_isUpsideDown != facing)
            flipGravity(player, !player->m_isUpsideDown);
    }

    player->spiderTestJumpInternal(false);
}

void startDashing(PlayerObject* player, DashRingObject* ring) {
    if (!player || !ring || player->m_isDashing)
        return;

    player->m_isDashing = true;
    player->m_lastLandTime = 0.0;
    player->m_isRotating = false;
    player->m_isBallRotating2 = false;
    player->m_isBallRotating = false;
    player->m_rotationSpeed = 0.0;
    player->m_dashRing = ring;
    player->m_dashStartTime = player->m_totalTime;

    float dashAngle = ring->getObjectRotation();
    if (ring->isFlipX())
        dashAngle += 180.f;
    if (std::fabs(ring->getRotationX() - ring->getRotationY()) > 179.f)
        dashAngle += 180.f;

    dashAngle = std::fmod(-dashAngle, 360.f);
    if (dashAngle < -180.f)
        dashAngle += 360.f;
    else if (dashAngle > 180.f)
        dashAngle -= 360.f;

    float sidewaysAngle = 0.f;
    if (player->m_isPlatformer) {
        sidewaysAngle = ring->m_dashSpeed * 5.77f;
    } else {
        if (player->m_isSideways)
            sidewaysAngle = -90.f;
        if (player->m_isGoingLeft)
            sidewaysAngle += 180.f;

        float angle = dashAngle + sidewaysAngle;
        if (angle < -180.f)
            angle += 360.f;
        else if (angle > 180.f)
            angle -= 360.f;

        if (std::fabs(angle) > 90.f) {
            float side = angle <= 0.f ? -180.f : 180.f;
            angle -= side;
        }

        dashAngle = std::clamp(angle, -70.f, 70.f) - sidewaysAngle;
        sidewaysAngle = 1.f;
    }

    CCPoint dir = pointForAngle(dashAngle * 0.01745329f) * sidewaysAngle;
    if (player->m_isSideways)
        std::swap(dir.x, dir.y);

    player->m_dashX = dir.x;
    player->m_dashY = dir.y;
    if (!player->m_isPlatformer) {
        double value = dir.y / std::fabs(dir.x == 0.f ? 1.f : dir.x);
        player->m_dashY = value;
        player->m_dashX = std::fabs(value);
    } else if (dir.x != 0.f) {
        player->doReversePlayer(dir.x < 0.f);
    }

    player->m_dashAngle = dashAngle;
    player->m_lastGroundedPos = CCPoint{0.f, 0.f};
}

void stopDashing(PlayerObject* player) {
    if (!player || !player->m_isDashing)
        return;

    player->m_isDashing = false;
    player->m_lastLandTime = 0.0;
    if (player->m_isPlatformer && player->m_dashRing) {
        CCPoint boosted = {
            static_cast<float>(player->m_dashX * player->m_dashRing->m_endBoost),
            static_cast<float>(player->m_dashY * player->m_dashRing->m_endBoost),
        };
        player->m_isAccelerating = true;
        player->m_yVelocity = boosted.y;
        player->m_platformerXVelocity = boosted.x;
        player->m_affectedByForces = true;

        if (player->m_dashRing->m_stopSlide) {
            player->m_isAccelerating = false;
            player->m_affectedByForces = false;
        }
    }
    player->m_dashRing = nullptr;

    if (player->m_isPlatformer) {
        float normal = std::sqrt(player->m_dashX * player->m_dashX + player->m_dashY * player->m_dashY);
        if (normal <= 17.3f)
            normal = (normal / 17.3f) * 1.5f + 0.5f;
        else
            normal = 2.f;

        float sizeMod = player->m_vehicleSize == 1.f ? 0.433333f : 0.333333f;
        float sidewaysMod = player->m_isSideways ? -1.f : 1.f;
        int gravityMod = player->m_isUpsideDown ? -0xb4 : 0xb4;
        int leftMod = player->m_isGoingLeft ? -1 : 1;
        player->m_rotationSpeed = (gravityMod * leftMod * sidewaysMod * player->m_gravityMod * normal) / sizeMod;
        player->m_isRotating = true;
    }

    if (player->m_isBall) {
        if (player->m_isPlatformer) {
            player->m_isRotating = false;
            player->m_isBallRotating = false;
            player->m_rotationSpeed = 0.0;
        }
        player->runBallRotation(1.0);
    }
}

void teleportPlayer(GJBaseGameLayer* layer, TeleportPortalObject* object, PlayerObject* player) {
    if (!layer || !object || !player)
        return;

    player->m_wasTeleported = true;
    CCPoint playerPos = player->getPosition();
    TeleportPortalObject* destinationPortal = object->m_orangePortal;
    if (object->m_orangePortal) {
        object->m_teleportYOffset = object->m_orangePortal->getStartPos().y - object->getStartPos().y;
    } else if (object->m_targetGroupID > 0) {
        if (auto* group = getGroup(layer, object->m_targetGroupID); group && group->count() > 0) {
            float pick = group->count() == 1 ? 0.f : static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            if (pick == 1.f)
                pick = 0.f;
            destinationPortal = typeinfo_cast<TeleportPortalObject*>(group->objectAtIndex(static_cast<int>(pick * group->count())));
        }
    }

    if (destinationPortal) {
        CCPoint destination = destinationPortal->getPosition();
        CCPoint portalPos = object->getPosition();
        player->m_lastPortalPos = object->getPosition();
        player->m_lastActivatedPortal = object;
        player->m_fallStartY = 0.f;
        if (object->m_objectID == 0x2eb) {
            destination = CCPoint{playerPos.x, object->getRealPosition().y + object->m_teleportYOffset};
        }

        if (object->m_saveOffset) {
            portalPos = object->getRealPosition();
            portalPos -= playerPos;
            destination -= portalPos;
        }
        if (object->m_ignoreX)
            destination.x = playerPos.x;
        if (object->m_ignoreY)
            destination.y = playerPos.y;

        player->setPosition(destination);
    }

    if (object->m_gravityMode > 0) {
        bool gravity = object->m_gravityMode == 2;
        if (object->m_gravityMode == 3)
            gravity = !player->m_isUpsideDown;
        flipGravity(player, gravity);
    }

    float forceAngle;
    if (destinationPortal) {
        float gravityForceMod = 90.f;
        int id = object->m_objectID;
        if (id == 0x26 || id == 0x2eb || id == 0x2ed || id == 0x810 || id == 0xb56)
            gravityForceMod = 180.f;

        forceAngle = (object->isFlipX() ? 180.f : 1.f) + (gravityForceMod - destinationPortal->getRotationX());
    } else {
        forceAngle = (object->isFlipX() ? 180.f : 1.f) - object->getRotationX();
    }

    if (object->m_redirectForceEnabled) {
        redirectPlayerForce(player, forceAngle, object->m_redirectForceMod, object->m_redirectForceMin, object->m_redirectForceMax);
    } else if (object->m_staticForceEnabled) {
        float force = object->m_staticForce;
        if (force == 0.f && !object->m_staticForceAdditive) {
            player->m_isAccelerating = false;
            player->m_yVelocity = 0.0;
            if (player->m_isPlatformer) {
                player->m_platformerXVelocity = 0.0;
                player->m_affectedByForces = false;
            }
        } else {
            CCPoint dir = pointForAngle(forceAngle * 0.01745329f);
            float length = dir.getLength();
            if (length > 0.f) {
                CCPoint forceVector = dir * (force / length);
                if (player->m_isSideways)
                    std::swap(forceVector.x, forceVector.y);

                player->m_isAccelerating = true;
                player->m_yVelocity = forceVector.y + (object->m_staticForceAdditive ? player->m_yVelocity : 0.0);
                if (player->m_isPlatformer) {
                    player->m_platformerXVelocity =
                        forceVector.x + (object->m_staticForceAdditive ? player->m_platformerXVelocity : 0.0);
                    player->m_affectedByForces = true;
                }
            }
        }
    }

    if (object->m_snapGround)
        player->m_lastGroundedPos = player->getPosition();
}

void ringJump(PlayerObject* player, RingObject* ring) {
    if (!player || !ring || player->m_isDead)
        return;
    if (player->m_ringRelatedSet.find(ring->m_uniqueID) != player->m_ringRelatedSet.end())
        return;
    if (!player->m_stateRingJump2 || player->m_isDashing || !player->m_stateJumpBuffered)
        return;

    bool custom = ring->m_objectType == GameObjectType::CustomRing;
    bool teleport = ring->m_objectType == GameObjectType::TeleportOrb;
    if ((player->m_touchedRing || custom || teleport) &&
        (player->m_touchedCustomRing || ring->m_objectType != GameObjectType::CustomRing)) {
        if (player->m_touchedGravityPortal || ring->m_objectType != GameObjectType::TeleportOrb)
            return;
    }

    ShowTrajectory::snapshotObject(ring);
    ShowTrajectory::rememberActivated(player, ring);

    if (ring->m_isReverse)
        player->reversePlayer(ring);

    player->m_ringJumpRelated = true;
    player->m_ringRelatedSet.insert(ring->m_uniqueID);
    if (custom)
        player->m_touchedCustomRing = true;
    else if (teleport)
        player->m_touchedGravityPortal = true;
    else
        player->m_touchedRing = true;

    player->m_touchingRings->removeObject(ring, true);
    if (!custom && !teleport)
        player->m_padRingRelated = true;

    if (custom)
        return;

    if (teleport) {
        teleportPlayer(player->m_gameLayer, reinterpret_cast<TeleportPortalObject*>(ring), player);
        return;
    }

    if (ring->m_objectType == GameObjectType::SpiderOrb) {
        bool facing = player->m_isSideways ? ring->isFacingLeft() : ring->isFacingDown();
        if (facing != player->m_isUpsideDown)
            flipGravity(player, !player->m_isUpsideDown);
        player->spiderTestJumpInternal(false);
        return;
    }

    if (ring->m_objectType == GameObjectType::DashRing) {
        startDashing(player, reinterpret_cast<DashRingObject*>(ring));
        return;
    }

    if (ring->m_objectType == GameObjectType::GravityDashRing) {
        flipGravity(player, !player->m_isUpsideDown);
        startDashing(player, reinterpret_cast<DashRingObject*>(ring));
        return;
    }

    player->m_stateRingJump = false;
    if (ring->m_objectType == GameObjectType::DropRing) {
        float velocity = gravitySign(player) * -15.f;
        if (player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing)
            velocity = gravitySign(player) * -14.f;
        if (player->m_isBird)
            velocity *= 0.8f;
        else if (!player->m_isRobot && player->m_isSpider)
            velocity *= 1.1f;
        player->setYVelocity(velocity, 1);
        if (!player->m_isBall && !player->m_isLocked && !player->m_isDashing) {
            player->m_isRotating = false;
            player->m_isBallRotating2 = false;
            player->m_isBallRotating = false;
            player->m_rotationSpeed = 0.0;
            player->runNormalRotation(0, 1.0);
        } else {
            player->runBallRotation2();
        }
        player->m_hasEverHitRing = true;
        player->m_isAccelerating = true;
        if (player->m_isBall || player->m_isSwing)
            player->m_jumpBuffered = false;
        return;
    }

    float yStart = player->m_yStart;
    if (ring->m_objectType == GameObjectType::GravityRing)
        yStart *= 0.8f;
    else if (ring->m_objectType == GameObjectType::GreenRing && player->m_isShip)
        yStart *= 0.7f;
    else if (ring->m_objectType == GameObjectType::PinkJumpRing)
        yStart *= player->m_isShip ? 0.37f : player->m_isBird ? 0.42f : player->m_isBall ? 0.77f : 0.72f;
    else if (ring->m_objectType == GameObjectType::RedJumpRing) {
        player->m_isAccelerating = true;
        yStart *= player->m_isShip ? 1.4f : player->m_isBird ? (player->m_vehicleSize == 1.f ? 1.02f : 1.36f) : player->m_isBall ? 1.34f : player->m_isRobot ? 1.28f : player->m_isSpider ? 1.34f : 1.38f;
    } else if (player->m_isRobot) {
        yStart *= 0.9f;
    }

    if (ring->m_objectType == GameObjectType::GreenRing)
        flipGravity(player, !player->m_isUpsideDown);

    float sizeGravity = player->m_vehicleSize != 1.f ? 0.8f : 1.f;
    player->setYVelocity(gravitySign(player) * yStart * sizeGravity, gravitySign(player));
    player->m_maybeIsBoosted = true;
    player->m_isOnGround2 = false;
    player->m_isOnGround = false;
    player->m_lastGroundedPos = player->getPosition();
    player->m_hasEverHitRing = true;

    if (!player->m_isBall && !player->m_isLocked && !player->m_isDashing) {
        player->m_isRotating = false;
        player->m_isBallRotating2 = false;
        player->m_isBallRotating = false;
        player->m_rotationSpeed = 0.0;
        player->runNormalRotation(0, 1.0);
    } else {
        player->runBallRotation2();
    }

    if (player->m_isSwing)
        player->m_yVelocity *= 0.6;
    else if (player->m_isBall || player->m_isSpider)
        player->m_yVelocity *= 0.7;

    if (ring->m_objectType == GameObjectType::GravityRing)
        flipGravity(player, !player->m_isUpsideDown);
    if (ring->m_objectType == GameObjectType::RedJumpRing)
        player->m_isAccelerating = true;
}

void togglePlayerScale(PlayerObject* player, bool smallSize) {
    if (!player)
        return;
    if (player->m_vehicleSize == 1.f && !smallSize)
        return;
    if (smallSize && player->m_vehicleSize != 1.f)
        return;

    if (smallSize) {
        player->m_vehicleSize = 0.6f;
    } else {
        if (player->m_isPlatformer)
            player->m_stateScale = 2;
        player->m_vehicleSize = 1.f;
    }

    player->m_spriteWidthScale = player->m_vehicleSize;
    player->m_spriteHeightScale = player->m_vehicleSize;
    player->setScaleX(player->m_vehicleSize);
    player->setScaleY(player->m_vehicleSize);
    player->updatePlayerScale();
}
}
