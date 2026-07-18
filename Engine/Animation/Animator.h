#pragma once

#include "AnimationSystem.h"
#include "Core/Handle.h"
#include <cstdint>
#include <vector>

enum class PlayMode : uint8_t {
    Replace = 0,
    Concurrent
};

struct PlayOptions {
    bool loop = false;
    float playback_speed = 1.0f;
    float blend_time = 0.0f;
    PlayMode mode = PlayMode::Replace;
};

class Animator {
    friend AnimationSystem;
public:
    void Play(AnimationHandle animation, const PlayOptions &options = {});
    void Queue(AnimationHandle animation, const PlayOptions &options = {});
    void Stop(float blend_time);
    void ClearQueue();

    bool IsPlaying(uint32_t index);
    bool IsPlayingAnything() const;
private:
    struct AnimationEntry {
        AnimationHandle animation;
        PlayOptions options;
        float timestamp = 0.0f;
    };
private:
    std::vector<AnimationHandle> m_animations;
    std::vector<AnimationEntry> m_active_animations;

    
};