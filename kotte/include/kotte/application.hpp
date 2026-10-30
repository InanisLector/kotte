#pragma once

#include "kotte/camera.hpp"
#include "kotte/entity.hpp"
#include "kotte/entity_store.hpp"
#include "kotte/random.hpp"
#include "kotte/renderer.hpp"
#include "kotte/spatial_grid.hpp"
#include "kotte/tile_map.hpp"
#include "kotte/window.hpp"

#include <cstddef>
#include <cstdint>
#include <raylib.h>
#include <string_view>
#include <vector>

namespace kotte
{
    struct Diagnostics final {
        std::size_t enemy_updates = 0;
        std::size_t movement_attempts = 0;
        std::size_t spatial_queries = 0;
        std::size_t cells_visited = 0;
        std::size_t candidate_references = 0;
        std::size_t unique_candidates = 0;
        std::size_t exact_tests = 0;
        std::size_t contacts = 0;
        std::size_t boundary_blocks = 0;
        std::size_t blocked_moves = 0;
        std::size_t enemy_turns = 0;
        std::size_t tiles_rendered = 0;
        std::size_t exact_visibility_tests = 0;
        std::size_t visible_entities = 0;
    };

    class Application final{
    public:
        Application(
            int window_width,
            int window_height,
            std::string_view title,
            std::uint64_t seed,
            int target_fps = 0);

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void run();
        void request_exit() noexcept;

    private:
        struct FrameActions final {
            Vector2 player_movement_direction{};
            bool place_bomb = false;
            bool place_unlit_bomb = false;
        };

        struct PlaceBombRequest final {
            EntityHandle owner;
            Vector2 world_position{};
            bool fuse_lit = true;
        };

        struct BombDetonated final {
            EntityHandle bomb;
            EntityHandle owner;
            Vector2 world_position{};
            std::uint8_t blast_range = 0;
        };

        struct DestroyEntityRequest final {
            EntityHandle entity;
        };

        struct BlastEffect final {
            std::vector<Vector2> tile_centres;
            float remaining_seconds = 0.0f;
        };

        enum GameState : uint8_t {
            ongoing,
            won,
            lost
        };

        class SelectionMenu final {
        private:
            bool restart_selected = true;
            float navigation_timer = 0;
            float navigation_cooldown = 0.15f;

        public:
            inline bool restart_is_selected() { return restart_selected; }
            inline void try_move() {
                if(navigation_timer <= 0) {
                    restart_selected = !restart_selected;
                    navigation_timer = navigation_cooldown;
                }
            }
            inline void tick(float delta_time) {
                if(navigation_timer > 0)
                    navigation_timer -= delta_time;
            }
        };

        void run_frame(float delta_time);
        void begin_frame() noexcept;
        void collect_frame_actions() noexcept;
        void update_gameplay(float delta_time);
        void update_timed_gameplay(float delta_time);
        void resolve_gameplay_facts(float delta_time);
        void apply_structural_mutations();
        void check_win_conditions();
        void update_presentation(float delta_time);
        void update_ui_status(float delta_time);
        void update_player(float delta_time);
        void try_move_player(Vector2 axis_displacement);
        void update_enemies(float delta_time);
        void queue_bomb_placement();
        void apply_entity_destructions();
        void apply_bomb_placements();
        void apply_bomb_lighting();
        [[nodiscard]] bool resolve_blast_tile(
            BlastEffect& effect,
            int column,
            int row);
        void append_blast_ray(
            BlastEffect& effect,
            int origin_column,
            int origin_row,
            int column_step,
            int row_step,
            std::uint8_t range);
        void update_camera(float delta_time) noexcept;
        void render_gameplay();
        void render_diagnostics();
        void render_restart_UI();
        void render_flush();
        void reset_game();
        PlayerData create_player();
        void populate_entities();
        void initialize_enemies();
        [[nodiscard]] Entity& entity(EntityHandle handle) noexcept;
        [[nodiscard]] const Entity& entity(EntityHandle handle) const noexcept;
        [[nodiscard]] Entity& player() noexcept;
        [[nodiscard]] const Entity& player() const noexcept;
        [[nodiscard]] Rectangle entity_world_bounds(const Entity& entity) const noexcept;
        [[nodiscard]] Rectangle entity_collision_bounds(const Entity& entity) const noexcept;
        [[nodiscard]] bool entity_movement_is_blocked(
            EntityHandle moving_entity_handle,
            Vector2 proposed_position);
        [[nodiscard]] static bool is_solid(EntityKind kind) noexcept;
        [[nodiscard]] static Vector2 cardinal_vector(CardinalDirection direction) noexcept;
        [[nodiscard]] CardinalDirection different_enemy_direction(CardinalDirection current_direction);
        [[nodiscard]] bool owner_has_active_bomb(EntityHandle owner) const noexcept;
        [[nodiscard]] static Vector2 tile_centre(Vector2 world_position) noexcept;
        [[nodiscard]] static Vector2 tile_centre(int column, int row) noexcept;
        [[nodiscard]] static float entity_roundness(EntityKind kind) noexcept;
        [[nodiscard]] Color entity_color(EntityKind kind) const noexcept;
        static constexpr int tile_size_ = 40;
        static constexpr int spatial_cell_tiles_ = 8;
        static constexpr float spatial_cell_size_ = tile_size_ * spatial_cell_tiles_;
        static constexpr float player_speed_ = 240.0f;
        static constexpr float enemy_speed_ = 80.0f;
        static constexpr float bomb_fuse_seconds_ = 2.0f;
        static constexpr std::uint8_t bomb_blast_range_ = 3;
        static constexpr float blast_effect_seconds_ = 0.35f;
        static constexpr Color background_color_{0x11, 0x22, 0x33, 0xff};
        static constexpr Color floor_colors_[3]{
            Color{0x38, 0x49, 0x52, 0xff},
            Color{0x3d, 0x4f, 0x59, 0xff},
            Color{0x42, 0x55, 0x60, 0xff}
        };
        static constexpr Color player_color_{0xf2, 0xc1, 0x4e, 0xff};
        static constexpr Color bomb_color_{0x20, 0x24, 0x2a, 0xff};
        static constexpr Color blast_color_{0xf2, 0x8f, 0x3b, 0xe8};
        static constexpr Color crate_color_{0xb0, 0x72, 0x3c, 0xff};
        static constexpr Color enemy_color_{0xd9, 0x4f, 0x70, 0xff};
        static constexpr Color pickup_color_{0xc6, 0xED, 0x47, 0xff};
        static constexpr Color unlocked_gate_color_{0xbf, 0xb4, 0xd6, 0xff};
        static constexpr Color locked_gate_color_{0x47, 0x06, 0x27, 0xff};
        static constexpr Color wall_color_{0x78, 0x5f, 0x47, 0xff};

        Window window_; // Constructed first, destroyed last.
        Camera camera_;
        Random random_;
        TileMap map_;
        EntityStore entities_;
        std::vector<EntityHandle> enemy_handles_;
        std::vector<EntityHandle> active_bomb_handles_;
        SpatialGrid spatial_grid_;
        Renderer renderer_;
        FrameActions frame_actions_;
        std::vector<PlaceBombRequest> place_bomb_requests_;
        std::vector<BombDetonated> bomb_detonated_facts_;
        std::vector<DestroyEntityRequest> destroy_entity_requests_;
        std::vector<EntityHandle> bombs_to_be_lit_;
        std::vector<BlastEffect> blast_effects_;
        Diagnostics diagnostics_;
        std::uint64_t seed_;
        PlayerData player_;
        EntityHandle gate_;
        GameState game_state_;
        bool gate_unlocked_ = false;
        SelectionMenu menu_ui_;
        bool exit_requested_ = false;
    };
}
