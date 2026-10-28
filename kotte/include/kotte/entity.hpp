#pragma once

#include "kotte/entity_handle.hpp"

#include <cstdint>
#include <optional>
#include <raylib.h>

namespace kotte
{
    enum class EntityKind : std::uint8_t {
        player,
        bomb,
        crate,
        enemy
    };

    enum class CardinalDirection : std::uint8_t {
        up,
        right,
        down,
        left
    };

    class PlayerData final {
    private:
        uint8_t _health;
        EntityHandle _handle;

    public:
        inline uint8_t GetHealth() const { return _health; }
        inline EntityHandle GetHandle() const { return _handle; }
        inline void DealDamage(uint8_t damage);

        PlayerData(EntityHandle playerHandle, uint8_t health = 10);
    };

    struct BombState final {
        EntityHandle owner;
        float fuse_remaining = 0.0f;
        std::uint8_t blast_range = 0;
        bool detonation_requested = false;
    };

    struct Entity final {
        EntityKind kind = EntityKind::crate;
        Vector2 world_position{};
        CardinalDirection movement_heading = CardinalDirection::right;
        std::optional<BombState> bomb_state;
    };
}
