#pragma once

#include "kotte/entity_handle.hpp"

#include <cstdint>
#include <optional>
#include <raylib.h>

#include "kid/event.hpp"

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
        uint8_t health_;
        uint8_t damage_ = 0;
        bool isAlive_ = true;
        EntityHandle handle_;

        float i_frame_timer_ = 0;
        float i_frame_duration_;

    public:
        kid::Event<> OnDeath;

        inline uint8_t get_health() const { return health_; }
        inline bool is_alive() const { return isAlive_; }
        inline EntityHandle get_handle() const { return handle_; }
        void deal_damage(uint8_t damage);
        void apply_update(float deltaTime);

        PlayerData(EntityHandle playerHandle, uint8_t health = 10, float iFrameDuration = 0.350);
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
        std::optional<BombState> bomb_state = std::nullopt;
    };
}
