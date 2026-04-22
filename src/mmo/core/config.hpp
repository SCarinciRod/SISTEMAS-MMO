#pragma once

#include <cstdint>
#include <string_view>

namespace mmo
{
    namespace core
    {
        namespace config
        {
            inline constexpr std::string_view project_name = "SISTEMAS-MMO";
            inline constexpr std::uint32_t server_tick_rate = 20;
            inline constexpr std::uint32_t max_world_players = 1024;
            inline constexpr std::uint32_t max_connections = 2048;
        }
    }
}
