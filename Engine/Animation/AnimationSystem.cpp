#include "AnimationSystem.h"
#include "Animator.h"

void AnimationSystem::Update(World &world, float delta_time) {
    world.Each<Animator>([&](Entity entity, Animator &animator) {
        // TODO
    });
}
