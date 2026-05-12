#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "mmo/core/item.hpp"

namespace mmo
{
    namespace persistence
    {
        enum class LuaLoadMode : std::uint8_t
        {
            strict = 0,
            permissive
        };

        struct LuaLoadOptions
        {
            // strict:
            // - invalid critical fields should fail the load.
            // - unknown kind/class/slot should fail.
            // - negative values for unsigned fields should fail.
            //
            // permissive:
            // - unknown optional fields may be ignored.
            // - missing optional fields keep default values.
            LuaLoadMode mode{ LuaLoadMode::strict };

            // Accept Lua files that return a table:
            //
            // return {
            //     [1] = { name = "Sword", kind = "equipment" }
            // }
            bool allow_returned_table{ true };

            // Accept Lua files that define a global `items` table:
            //
            // items = {
            //     [1] = { name = "Sword", kind = "equipment" }
            // }
            bool allow_global_items_table{ true };

            // If true, every item must have a valid name.
            bool require_name{ true };

            // If true, every item must have a valid kind.
            bool require_kind{ true };

            // If true, items with kind="equipment" must provide an equipment table.
            bool require_equipment_table_for_equipment{ true };

            // If true, a failed load should not leave the destination catalog partially modified.
            //
            // The implementation should load and validate first, then commit only after success.
            bool atomic{ true };
        };

        struct LuaLoadStats
        {
            std::uint32_t items_seen{ 0 };
            std::uint32_t items_loaded{ 0 };

            std::uint32_t equipment_loaded{ 0 };
            std::uint32_t materials_loaded{ 0 };
            std::uint32_t consumables_loaded{ 0 };
            std::uint32_t quest_items_loaded{ 0 };
            std::uint32_t collectibles_loaded{ 0 };
        };

        struct LuaLoadResult
        {
            bool ok{ false };
            std::string error{};
            LuaLoadStats stats{};
        };

        [[nodiscard]] inline auto lua_load_mode_name(LuaLoadMode mode) -> std::string_view
        {
            switch (mode)
            {
                case LuaLoadMode::strict:
                    return "strict";

                case LuaLoadMode::permissive:
                    return "permissive";
            }

            return "unknown";
        }

#ifdef MMO_USE_LUA

        [[nodiscard]] LuaLoadResult load_items_from_lua_ex(
            const std::string& path,
            core::item::Catalog& catalog,
            const LuaLoadOptions& options = LuaLoadOptions{});

        bool load_items_from_lua(
            const std::string& path,
            core::item::Catalog& catalog,
            std::string& out_error);

#else

        [[nodiscard]] inline LuaLoadResult load_items_from_lua_ex(
            const std::string&,
            core::item::Catalog&,
            const LuaLoadOptions& = LuaLoadOptions{})
        {
            LuaLoadResult result{};
            result.ok = false;
            result.error = "Lua support disabled (MMO_USE_LUA not defined)";
            return result;
        }

        inline bool load_items_from_lua(
            const std::string& path,
            core::item::Catalog& catalog,
            std::string& out_error)
        {
            const auto result = load_items_from_lua_ex(
                path,
                catalog,
                LuaLoadOptions{});

            out_error = result.error;
            return result.ok;
        }

#endif
    }
}