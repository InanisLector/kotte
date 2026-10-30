#include "kotte/application.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <utility>

namespace kotte
{    
    Application::Application(
        int width,
        int height,
        std::string_view title,
        std::uint64_t seed,
        int target_fps)
        : window_{width, height, title, target_fps}
        , camera_{window_.size()}
        , random_{seed}
        , map_{make_room(20, 12, random_)}
        , spatial_grid_{{
            static_cast<float>(map_.width() * tile_size_),
            static_cast<float>(map_.height() * tile_size_)}, spatial_cell_size_}
        , seed_{seed}
        , player_ {create_player()}
        , game_state_{GameState::ongoing} {
        random_ = Random(seed);
        populate_entities();
        initialize_enemies();
    }

    void Application::run(){
        while(!window_.should_close() && !exit_requested_){
            run_frame(GetFrameTime());
        }
    } 

    void Application::run_frame(float delta_time){
        begin_frame();
        collect_frame_actions();

        if(game_state_ == GameState::ongoing) {
            update_gameplay(delta_time);
            update_timed_gameplay(delta_time);
            resolve_gameplay_facts(delta_time);
            apply_structural_mutations();
            check_win_conditions();
            update_presentation(delta_time);

            render_gameplay();
            render_flush();
            render_diagnostics();
        } else {
            update_ui_status(delta_time);

            render_gameplay();
            render_restart_UI();
            render_flush();
            render_diagnostics();
        }
    }

    void Application::begin_frame() noexcept{
        // Every request must be consumed by the previous frame's mutation phase.
        assert(place_bomb_requests_.empty());
        assert(bomb_detonated_facts_.empty());
        assert(destroy_entity_requests_.empty());
        frame_actions_ = {};
        diagnostics_ = {};
    }

    void Application::collect_frame_actions() noexcept{
        if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)){
            request_exit();
        }
        frame_actions_.place_bomb = IsKeyPressed(KEY_SPACE);

        Vector2& movement_direction = frame_actions_.player_movement_direction;
        if(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)){
            movement_direction.x -= 1.0f;
        }
        if(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)){
            movement_direction.x += 1.0f;
        }
        if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)){
            movement_direction.y -= 1.0f;
        }
        if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)){
            movement_direction.y += 1.0f;
        }

        // Normalize intent before simulation so bindings cannot change movement speed.
        if(movement_direction.x != 0.0f || movement_direction.y != 0.0f){
            movement_direction = Vector2Normalize(movement_direction);
        }
    }

    void Application::update_gameplay(float delta_time){
        // The player and every enemy are movers; each asks its own collision question.
        update_player(delta_time);
        update_enemies(delta_time);
        queue_bomb_placement();
    }

    void Application::update_timed_gameplay(float delta_time){
        // This list is independent of camera queries, so off-screen fuses advance.
        for(const EntityHandle bomb_handle : active_bomb_handles_){
            Entity* bomb = entities_.try_get(bomb_handle);
            if(bomb == nullptr || !bomb->bomb_state.has_value()
                || bomb->bomb_state->detonation_requested){
                continue;
            }

            BombState& state = *bomb->bomb_state;
            state.fuse_remaining -= delta_time;
            if(state.fuse_remaining > 0.0f){
                continue;
            }

            state.fuse_remaining = 0.0f;
            state.detonation_requested = true;

            // The fact owns every value needed by later phases; it does not
            // borrow the bomb that will eventually request its own destruction.
            bomb_detonated_facts_.push_back({
                bomb_handle,
                state.owner,
                bomb->world_position,
                state.blast_range
            });
        }
    }

    void Application::resolve_gameplay_facts(float delta_time){
        for(const BombDetonated& fact : bomb_detonated_facts_){
            BlastEffect effect;
            effect.remaining_seconds = blast_effect_seconds_;
            effect.tile_centres.reserve(static_cast<std::size_t>(fact.blast_range) * 4 + 1);

            const float tile_extent = static_cast<float>(tile_size_);
            const int origin_column = static_cast<int>(std::floor(fact.world_position.x / tile_extent));
            const int origin_row = static_cast<int>(std::floor(fact.world_position.y / tile_extent));
            (void) resolve_blast_tile(effect, origin_column, origin_row);

            // The detonating bomb must disappear even if its presentation or
            // collision bounds change independently of blast-tile detection.
            destroy_entity_requests_.push_back({fact.bomb});

            append_blast_ray(effect, origin_column, origin_row, 0, -1, fact.blast_range);
            append_blast_ray(effect, origin_column, origin_row, 1, 0, fact.blast_range);
            append_blast_ray(effect, origin_column, origin_row, 0, 1, fact.blast_range);
            append_blast_ray(effect, origin_column, origin_row, -1, 0, fact.blast_range);

            blast_effects_.push_back(std::move(effect));
        }

        bomb_detonated_facts_.clear();

        auto bomblist = active_bomb_handles_;
        std::erase_if(bomblist, [this](EntityHandle h){return entity(h).bomb_state->owner_has_left;});

        for(const auto& bomb_handle : bomblist) {
            auto& bomb_entity = entity(bomb_handle);
            if(!CheckCollisionRecs(entity_collision_bounds(entity(bomb_entity.bomb_state->owner)), entity_collision_bounds(bomb_entity))) {
                bomb_entity.bomb_state->owner_has_left = true;
            }
        }

        auto query = spatial_grid_.query(entity_collision_bounds(player()));

        for(const auto& handle : query.entity_handles) {
            const auto& candidate = entity(handle);
            if(candidate.kind != EntityKind::pickup)
                continue;

            if(!CheckCollisionRecs(entity_collision_bounds(candidate), entity_collision_bounds(player())))
                continue;

            player_.pickup_upgrade();
            destroy_entity_requests_.push_back({handle});
        }

        player_.apply_update(delta_time);
    }

    void Application::apply_structural_mutations(){
        // Entity storage and every derived index change only at this boundary.
        apply_bomb_placements();
        apply_entity_destructions();
    }

    void Application::check_win_conditions() {
        if(CheckCollisionRecs(entity_collision_bounds(player()), entity_collision_bounds(entity(gate_))) && gate_unlocked_)
            game_state_ = GameState::won;
    }

    void Application::update_presentation(float delta_time){
        for(BlastEffect& effect : blast_effects_){
            effect.remaining_seconds -= delta_time;
        }
        std::erase_if(blast_effects_, [](const BlastEffect& effect){
            return effect.remaining_seconds <= 0.0f;
        });

        update_camera(delta_time);
    }

    
    void Application::update_ui_status(float delta_time) {
        menu_ui_.tick(delta_time);

        if(frame_actions_.player_movement_direction.x != 0)
            menu_ui_.try_move();

        if(frame_actions_.place_bomb) {
            if(menu_ui_.restart_is_selected())
                reset_game();
            else
                exit_requested_ = true;
        }
    }

    void Application::update_player(float delta_time){
        if(!player_.is_alive())
            return;

        const Vector2 direction = frame_actions_.player_movement_direction;

        Entity& player_entity = player();
        const Rectangle old_world_bounds = entity_world_bounds(player_entity);
        const Vector2 displacement = direction * (player_speed_ * delta_time);

        // Separate proposals let a clear axis slide along a blocked one.
        if(displacement.x != 0.0f){
            try_move_player({displacement.x, 0.0f});
        }
        if(displacement.y != 0.0f){
            try_move_player({0.0f, displacement.y});
        }

        const Rectangle player_bounds = entity_world_bounds(player_entity);

        const Vector2 bounds_offset = player_entity.world_position - Vector2{player_bounds.x, player_bounds.y};
        const float floor_left = static_cast<float>(tile_size_);
        const float floor_top = static_cast<float>(tile_size_);
        const float floor_right = static_cast<float>((map_.width() - 1) * tile_size_);
        const float floor_bottom = static_cast<float>((map_.height() - 1) * tile_size_);

        const Vector2 minimum_position{floor_left + bounds_offset.x, floor_top + bounds_offset.y};
        const Vector2 maximum_position{floor_right - bounds_offset.x, floor_bottom};
        player_entity.world_position = Vector2Clamp(player_entity.world_position, minimum_position, maximum_position);

        const Rectangle new_world_bounds = entity_world_bounds(player_entity);
        spatial_grid_.update(player_.get_handle(), old_world_bounds, new_world_bounds);
    }

    void Application::try_move_player(Vector2 axis_displacement){
        // Propose one horizontal or vertical player movement.
        ++diagnostics_.movement_attempts;

        Entity& player_entity = player();
        const Vector2 proposed_position = player_entity.world_position + axis_displacement;

        // Detect possible contacts using the mover's proposed collision bounds.
        if(entity_movement_is_blocked(player_.get_handle(), proposed_position)){
            // Respond by rejecting only this blocked player axis.
            ++diagnostics_.blocked_moves;
            return;
        }

        // Accept the clear proposal; grid synchronization happens after both axes.
        player_entity.world_position = proposed_position;
    }

    void Application::update_enemies(float delta_time){
        // The update list contains every enemy, not only those returned by the
        // camera query. Off-screen enemies therefore keep simulating normally.
        for(const EntityHandle enemy_handle : enemy_handles_){
            // Each enemy is one mover with one cardinal movement attempt.
            ++diagnostics_.enemy_updates;
            ++diagnostics_.movement_attempts;

            Entity& enemy = entity(enemy_handle);
            const Rectangle old_world_bounds = entity_world_bounds(enemy);
            // Propose one cardinal movement for this enemy.
            const Vector2 direction = cardinal_vector(enemy.movement_heading);
            const Vector2 displacement = direction * (enemy_speed_ * delta_time);
            const Vector2 proposed_position = enemy.world_position + displacement;

            // Detect a possible contact using this enemy's proposed bounds.
            if(entity_movement_is_blocked(enemy_handle, proposed_position)){
                // Respond by turning without moving; retry the new heading next update.
                ++diagnostics_.blocked_moves;
                ++diagnostics_.enemy_turns;
                enemy.movement_heading = different_enemy_direction(enemy.movement_heading);
                continue;
            }

            enemy.world_position = proposed_position;
            const Rectangle new_world_bounds = entity_world_bounds(enemy);

            // Synchronize this accepted move before the next enemy query.
            spatial_grid_.update(enemy_handle, old_world_bounds, new_world_bounds);
        }
    }

    void Application::queue_bomb_placement(){
        if(!frame_actions_.place_bomb){
            return;
        }

        // Capture values rather than an Entity reference because the request
        // deliberately outlives this simulation phase.
        place_bomb_requests_.push_back({
            player_.get_handle(),
            tile_centre(player().world_position)
        });
    }

    void Application::apply_bomb_placements(){
        for(const PlaceBombRequest& request : place_bomb_requests_){
            const Entity* owner = entities_.try_get(request.owner);
            if(owner == nullptr || owner->kind != EntityKind::player
                || owner_has_active_bomb(request.owner)){
                continue;
            }

            const Entity bomb{
                .kind = EntityKind::bomb,
                .world_position = request.world_position,
                .bomb_state = BombState{
                    .owner = request.owner,
                    .fuse_remaining = bomb_fuse_seconds_,
                    .blast_range = owner->kind == EntityKind::player ? player_.get_bomb_range() : bomb_blast_range_ // redundant, but alas
                }
            };

            const auto query = spatial_grid_.query(entity_collision_bounds(bomb));

            bool available_space = true;

            for(auto handle : query.entity_handles) {
                if(handle == request.owner)
                    continue;

                available_space &= !CheckCollisionRecs(entity_collision_bounds(bomb), entity_collision_bounds(entity(handle)));

                if(!available_space)
                    break;
            }

            if(!available_space)
                continue;

            const EntityHandle bomb_handle = entities_.create(bomb);

            // The store owns the bomb; these handles are synchronized, non-owning
            // views used by spatial queries and timed gameplay.
            spatial_grid_.insert(bomb_handle, entity_world_bounds(bomb));
            active_bomb_handles_.push_back(bomb_handle);
        }

        place_bomb_requests_.clear();
    }

    void Application::apply_entity_destructions(){
        // A moving entity may overlap several blast tiles, and several bombs may
        // reach it in one frame. Canonicalizing requests makes that multiplicity
        // explicit instead of depending on a forgiving destruction order.
        std::ranges::sort(destroy_entity_requests_, {}, &DestroyEntityRequest::entity);
        const auto duplicate_tail = std::ranges::unique(
            destroy_entity_requests_, {}, &DestroyEntityRequest::entity);
        destroy_entity_requests_.erase(duplicate_tail.begin(), duplicate_tail.end());

        for(const DestroyEntityRequest& request : destroy_entity_requests_){
            const Entity* doomed = entities_.try_get(request.entity);
            if(doomed == nullptr){
                continue;
            }

            // Spatial membership is derived from the live entity, so remove it
            // while the handle can still resolve to the bounds used at insertion.
            spatial_grid_.remove(request.entity, entity_world_bounds(*doomed));
            const bool destroyed = entities_.destroy(request.entity);
            assert(destroyed);
            (void) destroyed;
        }

        // These update lists are non-owning views. A single cleanup pass keeps
        // their iteration dense without teaching each destruction source about
        // every list that happens to reference an entity.
        std::erase_if(enemy_handles_, [this](EntityHandle handle){
            return !entities_.contains(handle);
        });

        gate_unlocked_ = enemy_handles_.size() == 0;

        std::erase_if(active_bomb_handles_, [this](EntityHandle handle){
            return !entities_.contains(handle);
        });

        destroy_entity_requests_.clear();
    }

    bool Application::resolve_blast_tile(
        BlastEffect& effect,
        int column,
        int row){
        const Vector2 centre = tile_centre(column, row);
        effect.tile_centres.push_back(centre);

        const float tile_extent = static_cast<float>(tile_size_);
        const Rectangle tile_bounds{
            static_cast<float>(column) * tile_extent,
            static_cast<float>(row) * tile_extent,
            tile_extent,
            tile_extent
        };
        const SpatialQuery contacts = spatial_grid_.query(tile_bounds);
        bool blocked_by_crate = false;

        for(const EntityHandle handle : contacts.entity_handles){
            const Entity* candidate = entities_.try_get(handle);
            if(candidate == nullptr
                || !CheckCollisionRecs(tile_bounds, entity_collision_bounds(*candidate))){
                continue;
            }

            switch(candidate->kind){
            case EntityKind::bomb:
            case EntityKind::pickup:
            case EntityKind::gate:
                break;
            case EntityKind::enemy:
                destroy_entity_requests_.push_back({handle});
                break;
            case EntityKind::crate:
                destroy_entity_requests_.push_back({handle});
                blocked_by_crate = true;
                break;
            case EntityKind::player:
                player_.deal_damage(2);
                break;
            }
        }

        return blocked_by_crate;
    }

    void Application::append_blast_ray(
        BlastEffect& effect,
        int origin_column,
        int origin_row,
        int column_step,
        int row_step,
        std::uint8_t range){
        for(int distance = 1; distance <= static_cast<int>(range); ++distance){
            const int column = origin_column + column_step * distance;
            const int row = origin_row + row_step * distance;
            if(!map_.contains(column, row) || map_.at(column, row).type == TileType::wall){
                break;
            }

            if(resolve_blast_tile(effect, column, row)){
                break;
            }
        }
    }

    void Application::update_camera(float delta_time) noexcept{
        (void) delta_time; // name the argument to avoid the static analyzer yelling at us. :) We'll animate the camera later.
        camera_.set_viewport_size(window_.size());

        if(player_.is_alive())
            camera_.set_centre(player().world_position);
    }

    void Application::render_gameplay(){
        Frame frame{background_color_};

        const Rectangle world_view = camera_.world_view();
        const TileRange visible_tiles = map_.tiles_overlapping(world_view, tile_size_);

        for(int y = visible_tiles.first_row; y < visible_tiles.past_last_row; ++y){
            for(int x = visible_tiles.first_column; x < visible_tiles.past_last_column; ++x){
                const Tile& tile = map_.at(x, y);
                const Color color = tile.type == TileType::wall
                    ? wall_color_
                    : floor_colors_[tile.variation % 3];

                const Vector2 tile_world_position{
                    static_cast<float>(x * tile_size_),
                    static_cast<float>(y * tile_size_)
                };
                const Vector2 tile_screen_position = camera_.world_to_screen(tile_world_position);

                renderer_.submit({{
                    std::floor(tile_screen_position.x),
                    std::floor(tile_screen_position.y),
                    tile_size_ - 1,
                    tile_size_ - 1},
                    color,
                    RenderLayer::floor});
                ++diagnostics_.tiles_rendered;
            }
        }

        const SpatialQuery spatial_query = spatial_grid_.query(world_view);

        // Blast effects are short-lived presentation values, not authoritative
        // entities. Rebuilding commands here keeps their lifetime out of gameplay.
        constexpr float blast_inset = 3.0f;
        for(const BlastEffect& effect : blast_effects_){
            for(const Vector2 tile_world_centre : effect.tile_centres){
                Rectangle bounds{
                    tile_world_centre.x - static_cast<float>(tile_size_) / 2.0f + blast_inset,
                    tile_world_centre.y - static_cast<float>(tile_size_) / 2.0f + blast_inset,
                    static_cast<float>(tile_size_) - blast_inset * 2.0f,
                    static_cast<float>(tile_size_) - blast_inset * 2.0f
                };
                if(!CheckCollisionRecs(bounds, world_view)){
                    continue;
                }

                const Vector2 screen_position = camera_.world_to_screen({bounds.x, bounds.y});
                bounds.x = screen_position.x;
                bounds.y = screen_position.y;
                renderer_.submit({
                    bounds,
                    blast_color_,
                    RenderLayer::effect,
                    tile_world_centre.y,
                    0.3f
                });
            }
        }

        // The spatial query replaces Week 3's complete entity scan. Its result is
        // only a candidate list, so every candidate still needs an exact test.
        for(const EntityHandle entity_handle : spatial_query.entity_handles){
            const Entity* visible_candidate = entities_.try_get(entity_handle);
            if(visible_candidate == nullptr){
                continue;
            }

            Rectangle bounds = entity_world_bounds(*visible_candidate);
            ++diagnostics_.exact_visibility_tests;
            if(!CheckCollisionRecs(bounds, world_view)){
                continue;
            }

            ++diagnostics_.visible_entities;
            const Vector2 screen_position = camera_.world_to_screen({bounds.x, bounds.y});
            bounds.x = screen_position.x;
            bounds.y = screen_position.y;
            const RenderLayer layer = visible_candidate->kind == EntityKind::bomb
                                    || visible_candidate->kind == EntityKind::pickup
                                    || visible_candidate->kind == EntityKind::gate
                ? RenderLayer::ground
                : RenderLayer::world;

            if(visible_candidate->kind == EntityKind::player) {
                renderer_.submit({
                    bounds,
                    entity_color(visible_candidate->kind),
                    layer,
                    visible_candidate->world_position.y,
                    entity_roundness(visible_candidate->kind),
                    std::format("HP: {}", player_.get_health())
                });
            } else {
                renderer_.submit({
                    bounds,
                    entity_color(visible_candidate->kind),
                    layer,
                    visible_candidate->world_position.y,
                    entity_roundness(visible_candidate->kind)
                });
            }
        }
    }

    void Application::render_diagnostics() {
        std::string diagnostics;
        const double percent_of_entities = entities_.live_count() == 0
            ? 0.0
            : 100.0 * static_cast<double>(diagnostics_.visible_entities) / static_cast<double>(entities_.live_count());

        // A naive collision loop would test every entity for every movement
        // attempt. Compare that full-scan count with the local broad-phase
        // candidates; this is candidate reduction, not a measured FPS gain.
        const double naive_collision_work = static_cast<double>(diagnostics_.movement_attempts)
            * static_cast<double>(entities_.live_count());
        const double broad_phase_reduction = naive_collision_work == 0.0
            ? 0.0
            : 100.0 * (1.0 - static_cast<double>(diagnostics_.unique_candidates) / naive_collision_work);

        DrawFPS(GetScreenWidth() - 100, 2);

        const double percent_of_world = map_.tile_count() == 0 ? 0.0 : 100.0 * static_cast<double>(diagnostics_.tiles_rendered) / static_cast<double>(map_.tile_count());
        diagnostics = std::format(
            "tiles rendered: {} ({:.1f}% of world)", diagnostics_.tiles_rendered, percent_of_world);
        DrawText(diagnostics.c_str(), 10, 10, 20, RAYWHITE);

        diagnostics = std::format(
            "entities: {} visible ({:.1f}% of {}) | {} visibility tests | {} commands",
            diagnostics_.visible_entities,
            percent_of_entities,
            entities_.live_count(),
            diagnostics_.exact_visibility_tests,
            renderer_.command_count());
        DrawText(diagnostics.c_str(), 10, 34, 20, RAYWHITE);

        diagnostics = std::format(
            "enemies updated: {} | active bombs: {} | blast effects: {}",
            diagnostics_.enemy_updates,
            active_bomb_handles_.size(),
            blast_effects_.size());
        DrawText(diagnostics.c_str(), 10, 58, 20, RAYWHITE);

        diagnostics = std::format(
            "broad phase: {} candidates | {:.1f}% fewer than full scan",
            diagnostics_.unique_candidates, broad_phase_reduction);
        DrawText(diagnostics.c_str(), 10, 82, 20, RAYWHITE);

        diagnostics = std::format(
            "narrow phase: {} exact tests | {} contacts",
            diagnostics_.exact_tests,
            diagnostics_.contacts);
        DrawText(diagnostics.c_str(), 10, 106, 20, RAYWHITE);

        diagnostics = std::format("seed: {}", seed_);
        DrawText(diagnostics.c_str(), 10, window_.height() - 78, 20, RAYWHITE);

        diagnostics = std::format(
            "world: {} x {} tiles | {} total", map_.width(), map_.height(), map_.tile_count());
        DrawText(diagnostics.c_str(), 10, window_.height() - 54, 20, RAYWHITE);

        DrawText("move: WASD/arrows | bomb: Space | quit: Q/Escape", 10, window_.height() - 30, 20, LIGHTGRAY);
    }

    void Application::render_restart_UI() {
        Rectangle restart {GetScreenWidth() / 3.0f - 50, GetScreenHeight() / 2.0f, 100, 20};
        Rectangle quit {GetScreenWidth() / 1.5f - 50, GetScreenHeight() / 2.0f, 100, 20};
        Rectangle state {GetScreenWidth() / 2.0f - 100, GetScreenHeight() / 1.5f, 200, 60};

        if(menu_ui_.restart_is_selected()) {
            renderer_.submitUI({restart, "Restart", 14, BLACK, WHITE, RED});
            renderer_.submitUI({quit, "Quit", 14, BLACK, WHITE, std::nullopt});
        } else {
            renderer_.submitUI({restart, "Restart", 14, BLACK, WHITE, std::nullopt});
            renderer_.submitUI({quit, "Quit", 14, BLACK, WHITE, RED});
        }

        std::string text;
        if(game_state_ == GameState::won)
            text = "You've won";
        else
            text = "You've lost";
        
        renderer_.submitUI({state, text, 30, BLACK, WHITE, BLACK});
    }

    void Application::render_flush() {
        renderer_.sort();
        renderer_.execute();
        renderer_.clear();
    }

    void Application::reset_game() {
        spatial_grid_.clear();
        entities_.clear();
        enemy_handles_.clear();
        active_bomb_handles_.clear();
        blast_effects_.clear();

        player_ = create_player();
        random_ = Random{ seed_ };
        gate_unlocked_ = false;
        game_state_ = GameState::ongoing;

        populate_entities();
        initialize_enemies();
    }

    PlayerData Application::create_player() {
        return PlayerData(entities_.create({EntityKind::player, {tile_size_ * 2.5f, tile_size_ * 2.5f}}), 10, 0.350);
    }

    void Application::populate_entities(){
        // Match the generation density so initial creation does not repeatedly
        // relocate slot storage. Handles remain valid even if this estimate grows.
        entities_.reserve(map_.tile_count() / 4);

        // Player

        spatial_grid_.insert(player_.get_handle(), entity_world_bounds(player()));
        player_.OnDeath.subscribe([this]{
                (void)this->destroy_entity_requests_.push_back({this->player_.get_handle()});
                this->game_state_ = GameState::lost;
            });

        auto pickup_handle = entities_.create(Entity(EntityKind::pickup, {tile_size_ * 1.5f, tile_size_ * 4.5f}));
        spatial_grid_.insert(std::move(pickup_handle), entity_world_bounds(entity(pickup_handle)));

        gate_ = entities_.create(Entity(EntityKind::gate, {tile_size_ * (map_.width() - 1.5f), tile_size_ * (map_.height() - 1.5f)}));
        spatial_grid_.insert(gate_, entity_world_bounds(entity(gate_)));

        for(int y = 1; y < map_.height() - 1; ++y){
            for(int x = 1; x < map_.width() - 1; ++x){
                if((x == 2 && y == 2) // player
                        || map_.at(x, y).type != TileType::floor 
                        || !random_.coin_flip(0.25f)
                        || (x == 2 && y == 5) // pick up
                        || (x == map_.width() - 2 && y == map_.height() - 2)){ 
                    continue;
                }

                const std::uint8_t kind_roll = random_.next<20, std::uint8_t>();
                // Week 3's inert bomb markers are no longer needed: every bomb
                // now enters the world through the explicit lifetime path.
                if(kind_roll >= 15 && kind_roll < 19){
                    continue;
                }
                const EntityKind kind = kind_roll < 15 ? EntityKind::crate : EntityKind::enemy;
                const Vector2 world_position{
                    static_cast<float>(x * tile_size_ + tile_size_ / 2),
                    static_cast<float>(y * tile_size_ + tile_size_ / 2)
                };
                const Entity new_entity{kind, world_position};
                const EntityHandle handle = entities_.create(new_entity);
                spatial_grid_.insert(handle, entity_world_bounds(new_entity));

                if(kind == EntityKind::enemy){
                    enemy_handles_.push_back(handle);
                }
            }
        }

        gate_unlocked_ = enemy_handles_.size() == 0;
    }

    void Application::initialize_enemies(){
        // Finish every world-generation roll before drawing runtime headings.
        // This preserves the complete Week 4 layout for the reference seed.
        for(const EntityHandle enemy_handle : enemy_handles_){
            Entity& enemy = entity(enemy_handle);
            enemy.movement_heading = static_cast<CardinalDirection>(
                random_.next<4, std::uint8_t>());
        }
    }

    Entity& Application::entity(EntityHandle handle) noexcept{
        Entity* resolved = entities_.try_get(handle);
        assert(resolved != nullptr);
        return *resolved;
    }

    const Entity& Application::entity(EntityHandle handle) const noexcept{
        const Entity* resolved = entities_.try_get(handle);
        assert(resolved != nullptr);
        return *resolved;
    }

    Entity& Application::player() noexcept{
        return entity(player_.get_handle());
    }

    const Entity& Application::player() const noexcept{
        return entity(player_.get_handle());
    }

    Rectangle Application::entity_world_bounds(const Entity& entity) const noexcept{
        Vector2 size{};
        switch(entity.kind){
        case EntityKind::player: size = {24.0f, 32.0f}; break;
        case EntityKind::bomb: size = {22.0f, 22.0f}; break;
        case EntityKind::crate: size = {36.0f, 48.0f}; break;
        case EntityKind::enemy: size = {28.0f, 30.0f}; break;
        case EntityKind::pickup: size = {22.0f, 22.0f}; break;
        case EntityKind::gate: size = {22.0f, 22.0f}; break;
        }

        const Vector2 top_left = entity.world_position - Vector2{size.x / 2.0f, size.y};
        return {top_left.x, top_left.y, size.x, size.y};
    }

    Rectangle Application::entity_collision_bounds(const Entity& entity) const noexcept{
        // Collision uses a smaller footprint around the part touching the
        // floor. The complete visual rectangle remains in the spatial grid so
        // broad-phase queries conservatively find every possible contact.
        Vector2 size{};
        switch(entity.kind){
        case EntityKind::player: size = {20.0f, 16.0f}; break;
        case EntityKind::bomb: size = {18.0f, 12.0f}; break;
        case EntityKind::crate: size = {32.0f, 32.0f}; break;
        case EntityKind::enemy: size = {24.0f, 14.0f}; break;
        case EntityKind::pickup: size = {18.0f, 12.0f}; break;
        case EntityKind::gate: size = {18.0f, 12.0f}; break;
        }

        const Vector2 top_left = entity.world_position - Vector2{size.x / 2.0f, size.y};
        return {top_left.x, top_left.y, size.x, size.y};
    }

    bool Application::entity_movement_is_blocked(
        EntityHandle moving_entity_handle,
        Vector2 proposed_position) {
        Entity proposed_entity = entity(moving_entity_handle);
        proposed_entity.world_position = proposed_position;

        // Room walls are a simple boundary rule in the current map. Test the
        // complete visual bounds directly before asking the grid about entities.
        const Rectangle proposed_world_bounds = entity_world_bounds(proposed_entity);
        const float floor_left = static_cast<float>(tile_size_);
        const float floor_top = static_cast<float>(tile_size_);
        const float floor_right = static_cast<float>((map_.width() - 1) * tile_size_);
        const float floor_bottom = static_cast<float>((map_.height() - 1) * tile_size_);
        const bool leaves_room = proposed_world_bounds.x < floor_left
            || proposed_world_bounds.y < floor_top
            || proposed_world_bounds.x + proposed_world_bounds.width > floor_right
            || proposed_world_bounds.y + proposed_world_bounds.height > floor_bottom;
        if(leaves_room){
            ++diagnostics_.boundary_blocks;
            return true;
        }

        const Rectangle proposed_collision_bounds = entity_collision_bounds(proposed_entity);

        // Broad phase: query nearby candidates using the proposed bounds.
        const SpatialQuery spatial_query = spatial_grid_.query(proposed_collision_bounds);
        ++diagnostics_.spatial_queries;
        diagnostics_.cells_visited += spatial_query.cells_visited;
        diagnostics_.candidate_references += spatial_query.candidate_references;
        // These counters aggregate work across movers, not globally unique entities.
        diagnostics_.unique_candidates += spatial_query.entity_handles.size();

        bool blocked = false;

        // Filter: remove the mover and non-solid kinds before exact testing.
        for(const EntityHandle candidate_handle : spatial_query.entity_handles){
            if(candidate_handle == moving_entity_handle){
                continue;
            }

            const Entity* candidate = entities_.try_get(candidate_handle);
            if(candidate == nullptr || !is_solid(candidate->kind)){
                continue;
            }

            // Bomb half-solid state
            if(candidate->kind == EntityKind::bomb) {
                assert(candidate->bomb_state.has_value());

                if((!candidate->bomb_state->owner_has_left) && candidate->bomb_state->owner == moving_entity_handle)
                    continue;
            }

            // Narrow phase: exact-test the remaining solid candidate.
            ++diagnostics_.exact_tests;
            if(CheckCollisionRecs(proposed_collision_bounds, entity_collision_bounds(*candidate))){
                bool player_collision = 
                    (proposed_entity.kind == EntityKind::enemy && candidate->kind == EntityKind::player) ||
                    (proposed_entity.kind == EntityKind::player && candidate->kind == EntityKind::enemy);

                if(player_collision)
                    player_.deal_damage(1);

                ++diagnostics_.contacts;
                blocked = true;
            }
        }

        return blocked;
    }

    bool Application::is_solid(EntityKind kind) noexcept{
        switch(kind){
        case EntityKind::player:
        case EntityKind::crate:
        case EntityKind::enemy:
        case EntityKind::bomb:
            return true;
        case EntityKind::pickup:
        case EntityKind::gate:
            return false;
        }

        return false;
    }

    Vector2 Application::cardinal_vector(CardinalDirection direction) noexcept{
        switch(direction){
        case CardinalDirection::up: return {0.0f, -1.0f};
        case CardinalDirection::right: return {1.0f, 0.0f};
        case CardinalDirection::down: return {0.0f, 1.0f};
        case CardinalDirection::left: return {-1.0f, 0.0f};
        }

        return {};
    }

    CardinalDirection Application::different_enemy_direction(CardinalDirection current_direction){
        constexpr std::uint8_t direction_count = 4;
        const std::uint8_t current_value = static_cast<std::uint8_t>(current_direction);

        // Adding 1, 2, or 3 modulo four reaches every heading except the one
        // that was just blocked.
        const std::uint8_t offset = static_cast<std::uint8_t>(
            random_.next<3, std::uint8_t>() + 1);
        const std::uint8_t new_value = static_cast<std::uint8_t>(
            (current_value + offset) % direction_count);
        return static_cast<CardinalDirection>(new_value);
    }

    bool Application::owner_has_active_bomb(EntityHandle owner) const noexcept{
        return std::ranges::any_of(active_bomb_handles_, [this, owner](EntityHandle bomb_handle){
            const Entity* bomb = entities_.try_get(bomb_handle);
            return bomb != nullptr
                && bomb->bomb_state.has_value()
                && bomb->bomb_state->owner == owner;
        });
    }

    Vector2 Application::tile_centre(Vector2 world_position) noexcept{
        const float tile_extent = static_cast<float>(tile_size_);
        const int column = static_cast<int>(std::floor(world_position.x / tile_extent));
        const int row = static_cast<int>(std::floor(world_position.y / tile_extent));
        return tile_centre(column, row);
    }

    Vector2 Application::tile_centre(int column, int row) noexcept{
        const float tile_extent = static_cast<float>(tile_size_);
        return {
            static_cast<float>(column) * tile_extent + tile_extent / 2.0f,
            static_cast<float>(row) * tile_extent + tile_extent / 2.0f
        };
    }

    Color Application::entity_color(EntityKind kind) const noexcept{
        switch(kind){
        case EntityKind::player: return player_color_;
        case EntityKind::bomb: return bomb_color_;
        case EntityKind::crate: return crate_color_;
        case EntityKind::enemy: return enemy_color_;
        case EntityKind::pickup: return pickup_color_;
        case EntityKind::gate: return gate_unlocked_
                               ? unlocked_gate_color_
                                :locked_gate_color_;
        }
        return MAGENTA;
    }

    float Application::entity_roundness(EntityKind kind) noexcept{
        return kind == EntityKind::crate ? 0.15f : 0.8f;
    }

    void Application::request_exit() noexcept{
        exit_requested_ = true;
    }

} //kotte
