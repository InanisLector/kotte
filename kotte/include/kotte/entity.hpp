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
        pickup,
        crate,
        enemy,
        gate
    };

    enum class CardinalDirection : std::uint8_t {
        up,
        right,
        down,
        left
    };

    class PlayerData final {
    private:
        std::uint8_t health_;
        std::uint8_t damage_ = 0;
        bool isAlive_ = true;
        EntityHandle handle_;

        std::uint8_t blast_range_ = 3;
        std::uint8_t number_of_concurent_unlit_bombs_ = 3;
        std::uint8_t number_of_concurent_unlit_bombs_modifier_ = 0;

        float i_frame_timer_ = 0;
        float i_frame_duration_;

    public:
        kid::Event<> OnDeath;
        uint8_t number_of_kills_since_last_bomb_ = 0;

        inline uint8_t get_health() const { return health_; }
        inline bool is_alive() const { return isAlive_; }
        inline EntityHandle get_handle() const { return handle_; }
        inline std::uint8_t get_bomb_range() const { return blast_range_; }
        inline std::uint8_t get_number_of_concurent_unlit_bombs() const { return number_of_concurent_unlit_bombs_ + number_of_concurent_unlit_bombs_modifier_; }
        inline void pickup_upgrade() { blast_range_++; }
        inline void kill_upgrade(uint8_t number_of_dead) { number_of_concurent_unlit_bombs_modifier_ =
            number_of_concurent_unlit_bombs_modifier_ > number_of_dead ?
            number_of_concurent_unlit_bombs_modifier_ : number_of_dead; }
        void deal_damage(uint8_t damage);
        void apply_update(float deltaTime);

        PlayerData(EntityHandle playerHandle, uint8_t health = 10, float iFrameDuration = 0.350);
    };

    struct BombState final {
        EntityHandle owner;
        float fuse_remaining = 0.0f;
        std::uint8_t blast_range = 0;
        bool detonation_requested = false;
        bool owner_has_left = false;
        bool fuse_lit = true;
    };

    struct Entity final {
        EntityKind kind = EntityKind::crate;
        Vector2 world_position{};
        CardinalDirection movement_heading = CardinalDirection::right;
        std::optional<BombState> bomb_state = std::nullopt;
    };
}
