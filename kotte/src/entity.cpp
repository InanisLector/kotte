#include "kotte/entity.hpp"

namespace kotte {
    PlayerData::PlayerData(EntityHandle playerHandle, uint8_t health, float iFrameDuration) : health_{health}, handle_{playerHandle}, i_frame_duration_{iFrameDuration} {};

    void PlayerData::deal_damage(uint8_t damage) {
        damage_ = damage > damage_ ? // Only store the highest damage for the tick
            damage :
            damage_;
    }

    void PlayerData::apply_update(float deltaTime) {
        if(i_frame_timer_ > 0) {
            i_frame_timer_ -= deltaTime;
            return;
        }

        if(!isAlive_)
            return;

        if(damage_ >= health_) {
            health_ = 0;
            isAlive_ = false;
            OnDeath.invoke();
        }
        else {
            health_ -= damage_;

            i_frame_timer_ = i_frame_duration_;
        }

        damage_ = 0;
    }
}
