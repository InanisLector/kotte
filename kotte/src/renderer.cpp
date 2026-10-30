#include "kotte/renderer.hpp"

#include <algorithm>
#include <raylib.h>

namespace kotte
{
    void Renderer::clear() noexcept{
        commands_.clear();
        UIcommands_.clear();
    }

    void Renderer::submit(RenderCommand command){
        commands_.push_back(command);
    }

    void Renderer::submitUI(UIRenderCommand command) {
        UIcommands_.push_back(command);
    }

    void Renderer::sort(){
        std::ranges::sort(commands_, [](const RenderCommand& left, const RenderCommand& right){
            if(left.layer != right.layer){
                return left.layer < right.layer;
            }
            return left.depth < right.depth;
        });
    }

    void Renderer::execute() const{
        for(const RenderCommand& command : commands_){
            DrawRectangleRounded(command.screen_bounds, command.roundness, 6, command.color);

            if(command.caption.has_value())
                DrawText(command.caption.value().c_str(),
                        command.screen_bounds.x - command.caption.value().size() * 1,
                        command.screen_bounds.y + command.screen_bounds.height,
                        8, RAYWHITE);
        }

        for(const UIRenderCommand& command : UIcommands_){
            if(command.background.has_value())
                DrawRectangleRec(command.screen_bounds, command.background.value());

            if(command.outline.has_value()) {
                DrawRectangleLines(command.screen_bounds.x, command.screen_bounds.y, command.screen_bounds.width, command.screen_bounds.height, command.outline.value());
                DrawRectangleLines(command.screen_bounds.x + 1, command.screen_bounds.y + 1, command.screen_bounds.width - 2, command.screen_bounds.height - 2, command.outline.value());
            }

            DrawText(command.text.c_str(), command.screen_bounds.x +  command.font_size * 0.5f, command.screen_bounds.y + command.font_size * 0.5f, command.font_size, command.text_color);
        }
    }

    std::size_t Renderer::command_count() const noexcept{
        return commands_.size();
    }
}
