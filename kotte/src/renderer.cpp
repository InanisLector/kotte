#include "kotte/renderer.hpp"

#include <algorithm>

namespace kotte
{
    void Renderer::clear() noexcept{
        commands_.clear();
    }

    void Renderer::submit(RenderCommand command){
        commands_.push_back(command);
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
    }

    std::size_t Renderer::command_count() const noexcept{
        return commands_.size();
    }
}
