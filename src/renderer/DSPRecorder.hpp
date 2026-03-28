#pragma once

#include <vector>
#include <asp/sync/Mutex.hpp>
#include <Geode/fmod/fmod.hpp>

#ifndef GEODE_IS_IOS

class DSPRecorder {
public:
    static DSPRecorder* get();

    void start();
    void stop();
    void stopBlocking();
    std::vector<float> getData();
    void tryUnpause(float time) const;

    [[nodiscard]] bool isRecording() const { return m_recording; }

private:
    void init();

    FMOD::DSP*          m_dsp         = nullptr;
    FMOD::ChannelGroup* m_masterGroup = nullptr;
    asp::Mutex<std::vector<float>> m_data;
    bool                m_recording   = false;
    // Signal for blocking stop
    bool m_stopComplete = false;
};

#endif