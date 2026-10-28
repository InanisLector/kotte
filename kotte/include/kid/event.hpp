#pragma once

#include <functional>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <cassert>
#include <utility>

namespace kid {
    template <typename... Args>
    class Event {
    public:
        using SubscriberID = std::size_t;

    private:
        using FunctionRef = std::function<void(Args...)>;

        std::vector<std::pair<SubscriberID, FunctionRef>> calllist_;
        SubscriberID currentId_ = 0;

    public:
        inline SubscriberID subscribe(FunctionRef function) {
            assert(currentId_ < std::numeric_limits<SubscriberID>::max());

            calllist_.emplace_back(currentId_, std::move<function>);
            return currentId_++;
        }

        // I'm aware this leaves gaps in IDs. I don't care enough.
        inline void unsubscribe(SubscriberID id) {
            calllist_.erase(
                    std::remove_if(calllist_.begin(), calllist_.end(),
                        [id](auto& pair) { return pair.first == id; }),
                    calllist_.end());
        
        }

        inline void invoke(Args... args) const {
            auto copy = calllist_;

            for (auto& [id, function] : copy) 
                function(args...);
        }

        inline void clear() {
            calllist_.clear();
            currentId_ = 0;
        }

        inline void reserve(size_t size) {
            calllist_.reserve(size);
        }

        inline void shrink_to_fit() {
            calllist_.shrink_to_fit();
        }
    };
}
