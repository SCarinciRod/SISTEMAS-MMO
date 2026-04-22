#pragma once

#include <cstddef>
#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace types
        {
            using Byte = std::uint8_t;
            using SignedByte = std::int8_t;
            using Word = std::uint16_t;
            using DWord = std::uint32_t;
            using QWord = std::uint64_t;
            using Index = std::size_t;

            using Float = float;
            using Double = double;
        }
    }
}
