#pragma once

#include "Core/Handle.h"
#include "Core/ResourcePool.h"
#include "World/Entity.h"
#include "World/World.h"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <variant>
#include <vector>
#include <glm/glm.hpp>

using AnimatedValue = std::variant<float, glm::vec2, glm::vec3, glm::vec4, glm::quat>;
struct AnimatedProperty {
    Entity entity;
    AnimatedValue value;
};

struct AnimationChannel {
    uint32_t sampler_index = 0;
    Entity entity;
    std::function<void(Entity &)> set_default;
    std::function<AnimatedValue(const Entity &)> sample_property;
    std::function<void(Entity &)> update_property;
};

struct Pose {
    std::vector<AnimatedProperty> animated_properties;
};

enum class AnimationInterpolation : uint8_t {
    Linear = 0,
    Step,
    CubicSpline
};

struct AnimationSampler {
    AnimationInterpolation interpolation;
    std::vector<float> times {};
    std::vector<AnimatedValue> output {};

    AnimatedValue Sample(float time) const;
};

struct Animation {
    std::string name {};
    std::vector<AnimationChannel> channels {};
    std::vector<AnimationSampler> samplers {};

    float duration = 0.0f;
};

class AnimationSystem {
public:
    AnimationHandle CreateAnimation(Animation &animation);
    Animation &Get(Animation &handle);

    void Update(World &world, float delta_time);
private:
    ResourcePool<Animation, AnimationHandleTag> m_animation_clips;
private:
    Pose CalculatePose(const World &world, const Animation &animation, float time);
    void SetPose(World &world, const Pose &pose);
    Pose BlendPoses(const Pose &pose1, const Pose &pose2, float weight);
};
