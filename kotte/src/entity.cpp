#include "kotte/entity.hpp"

namespace kotte {
    PlayerData::PlayerData(EntityHandle playerHandle, uint8_t health) : _health{health}, _handle{playerHandle} {};

    void PlayerData::DealDamage(uint8_t damage) {
        if(damage > _health)
            _health = 0;
        else
            _health -= damage;
    }
}
