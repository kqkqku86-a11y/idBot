#include "checkpoint.hpp"
#include "practice_fixes.hpp"
#include <Geode/modify/PlayLayer.hpp>

void resetTPSBypassState();

struct PracticeCheckpointData {
    SupplementalPlayerState    p1, p2;
    SupplementalPlayLayerState pl;
    
    PracticeCheckpointData() = default;
    PracticeCheckpointData(PlayerObject* p1Obj, PlayerObject* p2Obj, PlayLayer* plObj) {
        if (!plObj || !p1Obj) return;
        pl = SupplementalPlayLayerState(plObj);
        p1 = SupplementalPlayerState(p1Obj);
        if (p2Obj) p2 = SupplementalPlayerState(p2Obj);
    }
    
    void apply(PlayerObject* p1Obj, PlayerObject* p2Obj, PlayLayer* plObj) const {
        if (!plObj) return;
        pl.apply(plObj);
        if (p1Obj) p1.apply(p1Obj);
        if (p2Obj) p2.apply(p2Obj);
        
        // For platformer checkpoints, RobTop explicitly sets m_isOnGround = false
        // (see functions.txt lines 3192-3196). Don't override this with supplemental state.
        // Also clear m_jumpBuffered because releaseAllButtons (functions.txt:3193,3201)
        // does NOT clear it, causing auto-jump on respawn if it was true at checkpoint creation.
        if (plObj->m_isPlatformer) {
            if (p1Obj) {
                p1Obj->m_isOnGround = false;
                p1Obj->m_jumpBuffered = false;
            }
            if (p2Obj) {
                p2Obj->m_isOnGround = false;
                p2Obj->m_jumpBuffered = false;
            }
        }
    }
};

class $modify(FixPlayLayer, PlayLayer) {
    struct Fields {
        std::unordered_map<CheckpointObject*, PracticeCheckpointData> m_checkpoints;
        std::unordered_map<CheckpointObject*, std::vector<input>> m_checkpointInputs;
        std::unordered_map<CheckpointObject*, std::vector<gdr_legacy::FrameFix>> m_checkpointFrameFixes;
        std::unordered_map<CheckpointObject*, int> m_checkpointFrames;
    };
    
    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        auto& g = Global::get();
        bool wasRecordingOrPlaying = (g.state == state::recording || g.state == state::playing);
        bool shouldFix = PracticeFix::shouldEnable();

        log::debug("[PracticeFix] loadFromCheckpoint called. checkpoint={} shouldFix={} wasRecordingOrPlaying={}",
            (void*)checkpoint, shouldFix, wasRecordingOrPlaying);
        
        if (shouldFix) {
            if (m_player1) m_player1->m_isDashing = false;
            if (m_gameState.m_isDualMode && m_player2) m_player2->m_isDashing = false;
        }

        // Snapshot pre-restore state for comparison
        double preXVel = m_player1 ? m_player1->m_platformerXVelocity : 0.0;
        bool preMovingLeft = m_player1 ? m_player1->m_platformerMovingLeft : false;
        bool preMovingRight = m_player1 ? m_player1->m_platformerMovingRight : false;
        bool preHoldingLeft = m_player1 ? m_player1->m_holdingLeft : false;
        bool preHoldingRight = m_player1 ? m_player1->m_holdingRight : false;
        bool preIsMoving = m_player1 ? m_player1->m_isMoving : false;
        
        PlayLayer::loadFromCheckpoint(checkpoint);

        // Snapshot post-GD-restore state
        double postXVel = m_player1 ? m_player1->m_platformerXVelocity : 0.0;
        bool postMovingLeft = m_player1 ? m_player1->m_platformerMovingLeft : false;
        bool postMovingRight = m_player1 ? m_player1->m_platformerMovingRight : false;
        bool postHoldingLeft = m_player1 ? m_player1->m_holdingLeft : false;
        bool postHoldingRight = m_player1 ? m_player1->m_holdingRight : false;
        bool postIsMoving = m_player1 ? m_player1->m_isMoving : false;

        log::debug("[PracticeFix] After GD restore:"
            " xVel {:.4f}->{:.4f}"
            " movLeft {}>{}"
            " movRight {}>{}"
            " holdLeft {}>{}"
            " holdRight {}>{}"
            " isMoving {}>{}",
            preXVel, postXVel,
            preMovingLeft, postMovingLeft,
            preMovingRight, postMovingRight,
            preHoldingLeft, postHoldingLeft,
            preHoldingRight, postHoldingRight,
            preIsMoving, postIsMoving);
        
        resetTPSBypassState();
        
        auto* fields = m_fields.self();
        
        if (shouldFix) {
            auto it = fields->m_checkpoints.find(checkpoint);
            if (it != fields->m_checkpoints.end()) {
                log::debug("[PracticeFix] Found checkpoint data, applying supplemental state.");

                // Snapshot what supplemental is about to restore
                log::debug("[PracticeFix] Supplemental will restore:"
                    " xVel={:.4f} movLeft={} movRight={} holdLeft={} holdRight={} isMoving={} touchedPad={} jumpBuffered={}",
                    it->second.p1.m_platformerXVelocity,
                    it->second.p1.m_platformerMovingLeft,
                    it->second.p1.m_platformerMovingRight,
                    it->second.p1.m_holdingLeft,
                    it->second.p1.m_holdingRight,
                    it->second.p1.m_isMoving,
                    it->second.p1.m_touchedPad,
                    it->second.p1.m_jumpBuffered);

                it->second.apply(
                    m_player1,
                    m_gameState.m_isDualMode ? m_player2 : nullptr,
                    this
                );

                // Fix platformer state conflicts after RobTop's loadFromCheckpoint
                // RobTop explicitly releases all buttons and zeros velocity for platformer checkpoints
                // (see functions.txt lines 3192-3202). We need to restore button state.
                if (m_isPlatformer && m_player1) {
                    // Re-apply holding button state that RobTop's releaseAllButtons cleared
                    if (it->second.p1.m_holdingLeft) {
                        m_player1->m_holdingButtons[2] = true;
                    }
                    if (it->second.p1.m_holdingRight) {
                        m_player1->m_holdingButtons[3] = true;
                    }
                    // Restore isMoving flag based on velocity
                    if (m_player1->m_platformerXVelocity != 0.0) {
                        m_player1->m_isMoving = true;
                    }
                    // Validate moving flags don't conflict with holding flags
                    if (m_player1->m_holdingLeft && m_player1->m_platformerMovingRight) {
                        m_player1->m_platformerMovingRight = false;
                    }
                    if (m_player1->m_holdingRight && m_player1->m_platformerMovingLeft) {
                        m_player1->m_platformerMovingLeft = false;
                    }
                }
                if (m_gameState.m_isDualMode && m_player2 && m_isPlatformer) {
                    if (it->second.p2.m_holdingLeft) {
                        m_player2->m_holdingButtons[2] = true;
                    }
                    if (it->second.p2.m_holdingRight) {
                        m_player2->m_holdingButtons[3] = true;
                    }
                    if (m_player2->m_platformerXVelocity != 0.0) {
                        m_player2->m_isMoving = true;
                    }
                }

                // Snapshot post-supplemental state
                double suppXVel = m_player1 ? m_player1->m_platformerXVelocity : 0.0;
                bool suppMovingLeft = m_player1 ? m_player1->m_platformerMovingLeft : false;
                bool suppMovingRight = m_player1 ? m_player1->m_platformerMovingRight : false;
                bool suppHoldingLeft = m_player1 ? m_player1->m_holdingLeft : false;
                bool suppHoldingRight = m_player1 ? m_player1->m_holdingRight : false;
                bool suppIsMoving = m_player1 ? m_player1->m_isMoving : false;

                log::debug("[PracticeFix] After supplemental apply:"
                    " xVel={:.4f} movLeft={} movRight={} holdLeft={} holdRight={} isMoving={}",
                    suppXVel, suppMovingLeft, suppMovingRight,
                    suppHoldingLeft, suppHoldingRight, suppIsMoving);
            } else {
                log::warn("[PracticeFix] Checkpoint NOT found in m_checkpoints map! ptr={} map size={}",
                    (void*)checkpoint, fields->m_checkpoints.size());
            }
        }
        
        if (wasRecordingOrPlaying) {
            auto inputIt = fields->m_checkpointInputs.find(checkpoint);
            if (inputIt != fields->m_checkpointInputs.end()) {
                log::debug("[PracticeFix] Found checkpoint inputs ({} inputs), restoring.",
                    inputIt->second.size());

                g.ignoreRecordAction = true;
                
                if (g.state == state::recording) {
                    g.macro.inputs = inputIt->second;
                }
                
                auto frameIt = fields->m_checkpointFrames.find(checkpoint);
                if (frameIt != fields->m_checkpointFrames.end()) {
                    int savedFrame = frameIt->second;
                    g.m_frameCount = savedFrame;
                    int targetFrame = g.m_frameCount - g.frameOffset;

                    log::debug("[PracticeFix] Restoring frameCount={} targetFrame={}",
                        savedFrame, targetFrame);
                    
                    if (g.state == state::recording) {
                        auto sizeBefore = g.macro.inputs.size();
                        g.macro.inputs.erase(
                            std::remove_if(g.macro.inputs.begin(), g.macro.inputs.end(),
                            [targetFrame](const input& inp) { return inp.frame >= targetFrame; }),
                            g.macro.inputs.end()
                        );
                        log::debug("[PracticeFix] Erased inputs >= frame {}: {} -> {}",
                            targetFrame, sizeBefore, g.macro.inputs.size());
                    }
                    
                    g.currentAction = 0;
                    while (g.currentAction < g.macro.inputs.size() && g.macro.inputs[g.currentAction].frame < targetFrame) {
                        g.currentAction++;
                    }
                    g.currentFrameFix = 0;
                    while (g.currentFrameFix < g.macro.frameFixes.size() && g.macro.frameFixes[g.currentFrameFix].frame < targetFrame) {
                        g.currentFrameFix++;
                    }

                    log::debug("[PracticeFix] currentAction={} currentFrameFix={}",
                        g.currentAction, g.currentFrameFix);
                } else {
                    log::warn("[PracticeFix] Frame data NOT found for checkpoint ptr={}",
                        (void*)checkpoint);
                }
                
                if (g.state == state::recording) {
                    auto fixIt = fields->m_checkpointFrameFixes.find(checkpoint);
                    if (fixIt != fields->m_checkpointFrameFixes.end()) {
                        g.macro.frameFixes = fixIt->second;
                        int targetFrame = g.m_frameCount - g.frameOffset;
                        g.currentFrameFix = 0;
                        while (g.currentFrameFix < g.macro.frameFixes.size() && g.macro.frameFixes[g.currentFrameFix].frame < targetFrame) {
                            g.currentFrameFix++;
                        }
                        log::debug("[PracticeFix] Restored {} frameFixes, currentFrameFix={}",
                            g.macro.frameFixes.size(), g.currentFrameFix);
                    } else {
                        log::warn("[PracticeFix] FrameFixes NOT found for checkpoint ptr={}",
                            (void*)checkpoint);
                    }
                }
                
                g.ignoreRecordAction = false;
            } else {
                log::warn("[PracticeFix] Checkpoint inputs NOT found! ptr={} map size={}",
                    (void*)checkpoint, fields->m_checkpointInputs.size());
            }
        }
    }
    
    CheckpointObject* createCheckpoint() {
        auto* checkpoint = PlayLayer::createCheckpoint();
        if (!checkpoint) {
            log::warn("[PracticeFix] createCheckpoint returned null!");
            return checkpoint;
        }

        bool shouldFix = PracticeFix::shouldEnable();
        log::debug("[PracticeFix] createCheckpoint ptr={} shouldFix={}", (void*)checkpoint, shouldFix);

        if (shouldFix) {
            auto* fields = m_fields.self();
            fields->m_checkpoints[checkpoint] = PracticeCheckpointData(
                m_player1,
                m_gameState.m_isDualMode ? m_player2 : nullptr,
                this
            );

            // Log what we saved
            auto& saved = fields->m_checkpoints[checkpoint];
            log::debug("[PracticeFix] Saved supplemental state:"
                " xVel={:.4f} movLeft={} movRight={} holdLeft={} holdRight={} isMoving={} touchedPad={} jumpBuffered={}",
                saved.p1.m_platformerXVelocity,
                saved.p1.m_platformerMovingLeft,
                saved.p1.m_platformerMovingRight,
                saved.p1.m_holdingLeft,
                saved.p1.m_holdingRight,
                saved.p1.m_isMoving,
                saved.p1.m_touchedPad,
                saved.p1.m_jumpBuffered);
        }
        
        auto& g = Global::get();
        if (g.state == state::recording || g.state == state::playing) {
            auto* fields = m_fields.self();
            g.ignoreRecordAction = true;
            fields->m_checkpointInputs[checkpoint]     = g.macro.inputs;
            fields->m_checkpointFrameFixes[checkpoint] = g.macro.frameFixes;
            fields->m_checkpointFrames[checkpoint]     = Global::getCurrentFrame();
            g.ignoreRecordAction = false;

            log::debug("[PracticeFix] Saved frame={} inputs={} frameFixes={}",
                fields->m_checkpointFrames[checkpoint],
                fields->m_checkpointInputs[checkpoint].size(),
                fields->m_checkpointFrameFixes[checkpoint].size());
        }
        
        return checkpoint;
    }
    
    void removeCheckpoint(CheckpointObject* checkpoint) {
        log::debug("[PracticeFix] removeCheckpoint ptr={}", (void*)checkpoint);
        PlayLayer::removeCheckpoint(checkpoint);
        
        auto* fields = m_fields.self();
        fields->m_checkpoints.erase(checkpoint);
        fields->m_checkpointInputs.erase(checkpoint);
        fields->m_checkpointFrameFixes.erase(checkpoint);
        fields->m_checkpointFrames.erase(checkpoint);
    }
    
    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;

        log::debug("[PracticeFix] resetLevel hadCheckpoints={}", hadCheckpoints);
        
        if (!hadCheckpoints) {
            auto* fields = m_fields.self();
            fields->m_checkpoints.clear();
            fields->m_checkpointInputs.clear();
            fields->m_checkpointFrameFixes.clear();
            fields->m_checkpointFrames.clear();
        }
        
        PlayLayer::resetLevel();
        
        if (hadCheckpoints) m_resumeTimer = 0;
    }
};