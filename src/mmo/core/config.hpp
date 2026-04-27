#pragma once

#include "compat.hpp"

#include <cstdint>
#include <string_view>

namespace mmo
{
    namespace core
    {
        namespace config
        {
            constexpr std::string_view project_name = "SISTEMAS-MMO";
            constexpr std::uint32_t server_tick_rate = 20;
            constexpr std::uint32_t max_world_players = 1024;
            constexpr std::uint32_t max_connections = 2048;
        }
    }
}
