#pragma once

#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace damage
        {
            enum class Kind : std::uint8_t
            {
                slash = 0,
                pierce,
                sever,
                impact,
                magic,
                elemental
            };

            struct Packet
            {
                Kind kind{ Kind::impact };
                std::int32_t amount{ 0 };
                std::int32_t penetration_percent{ 0 };
                bool can_ricochet{ false };
                bool can_embed{ false };
            };

            [[nodiscard]] constexpr auto is_physical(Kind kind) -> bool
            {
                switch (kind)
                {
                    case Kind::magic:
                    case Kind::elemental:
                        return false;
                    default:
                        return true;
                }
            }
        }
    }
}