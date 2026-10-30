#pragma once

#include <cstddef>
#include <cstdint>
#include <raylib.h>
#include <string>
#include <vector>
#include <optional>

namespace kotte
{
    enum class RenderLayer : std::uint8_t {
        floor,
        ground,
        effect,
        world
    };

    struct RenderCommand final {
        Rectangle screen_bounds{};
        Color color{};
        RenderLayer layer = RenderLayer::world;
        float depth = 0.0f;
        float roundness = 0.0f;
        std::optional<std::string> caption = std::nullopt;
    };

    struct UIRenderCommand final {
        Rectangle screen_bounds {};
        std::string text {};
        float font_size = 14;
        Color text_color = BLACK;
        std::optional<Color> background = std::nullopt;
        std::optional<Color> outline = std::nullopt;
    };

    class Renderer final {
    public:
        void clear() noexcept;
        void submit(RenderCommand command);
        void submitUI(UIRenderCommand command);
        void sort();
        void execute() const;
        [[nodiscard]] std::size_t command_count() const noexcept;

    private:
        std::vector<RenderCommand> commands_;
        std::vector<UIRenderCommand> UIcommands_;
    };
}
