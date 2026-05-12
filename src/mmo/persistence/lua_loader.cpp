#include "mmo/persistence/lua_loader.hpp"

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef MMO_USE_LUA

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace mmo
{
    namespace persistence
    {
            struct ParsedItem
            {
                core::item::Definition definition{};
                std::string name{};
            };

            // Temporary/simple lifetime solution:
            // item definitions appear to store the name as string_view.
            // Therefore names loaded from Lua must live after this function returns.
            //
            // Long-term better solution:
            // Catalog should own item names, or item identity should use std::string.
            static std::vector<std::string> s_persistent_strings;

            [[nodiscard]] auto make_error(const std::string& message, const LuaLoadStats& stats) -> LuaLoadResult
            {
                LuaLoadResult result{};
                result.ok = false;
                result.error = message;
                result.stats = stats;
                return result;
            }

            [[nodiscard]] auto make_success(const LuaLoadStats& stats) -> LuaLoadResult
            {
                LuaLoadResult result{};
                result.ok = true;
                result.error.clear();
                result.stats = stats;
                return result;
            }

            [[nodiscard]] auto strict_mode(const LuaLoadOptions& options) -> bool
            {
                return options.mode == LuaLoadMode::strict;
            }

            [[nodiscard]] auto error_for_item(lua_Integer key, const std::string& message) -> std::string
            {
                std::ostringstream stream;
                stream << "item id " << static_cast<long long>(key) << ": " << message;
                return stream.str();
            }

            [[nodiscard]] auto read_string_field(
                lua_State* state,
                int table_index,
                const char* field_name,
                std::string& value) -> bool
            {
                lua_getfield(state, table_index, field_name);

                const bool found = lua_isstring(state, -1) != 0;

                if (found)
                {
                    const char* text = lua_tostring(state, -1);
                    value = text != nullptr ? text : "";
                }

                lua_pop(state, 1);
                return found;
            }

            [[nodiscard]] auto read_bool_field(
                lua_State* state,
                int table_index,
                const char* field_name,
                bool& value) -> bool
            {
                lua_getfield(state, table_index, field_name);

                const bool found = lua_isboolean(state, -1) != 0;

                if (found)
                {
                    value = lua_toboolean(state, -1) != 0;
                }

                lua_pop(state, 1);
                return found;
            }

            [[nodiscard]] auto read_int32_field(
                lua_State* state,
                int table_index,
                const char* field_name,
                std::int32_t& value,
                const LuaLoadOptions& options,
                std::string& error) -> bool
            {
                lua_getfield(state, table_index, field_name);

                if (lua_isnil(state, -1))
                {
                    lua_pop(state, 1);
                    return true;
                }

                if (!lua_isinteger(state, -1))
                {
                    if (strict_mode(options))
                    {
                        error = std::string("field must be integer: ") + field_name;
                        lua_pop(state, 1);
                        return false;
                    }

                    lua_pop(state, 1);
                    return true;
                }

                const lua_Integer raw = lua_tointeger(state, -1);

                if (raw < static_cast<lua_Integer>(std::numeric_limits<std::int32_t>::min()) ||
                    raw > static_cast<lua_Integer>(std::numeric_limits<std::int32_t>::max()))
                {
                    error = std::string("field out of int32 range: ") + field_name;
                    lua_pop(state, 1);
                    return false;
                }

                value = static_cast<std::int32_t>(raw);

                lua_pop(state, 1);
                return true;
            }

            [[nodiscard]] auto read_uint32_field(
                lua_State* state,
                int table_index,
                const char* field_name,
                std::uint32_t& value,
                const LuaLoadOptions& options,
                std::string& error) -> bool
            {
                lua_getfield(state, table_index, field_name);

                if (lua_isnil(state, -1))
                {
                    lua_pop(state, 1);
                    return true;
                }

                if (!lua_isinteger(state, -1))
                {
                    if (strict_mode(options))
                    {
                        error = std::string("field must be unsigned integer: ") + field_name;
                        lua_pop(state, 1);
                        return false;
                    }

                    lua_pop(state, 1);
                    return true;
                }

                const lua_Integer raw = lua_tointeger(state, -1);

                if (raw < 0)
                {
                    if (strict_mode(options))
                    {
                        error = std::string("field cannot be negative: ") + field_name;
                        lua_pop(state, 1);
                        return false;
                    }

                    lua_pop(state, 1);
                    return true;
                }

                if (static_cast<unsigned long long>(raw) > std::numeric_limits<std::uint32_t>::max())
                {
                    error = std::string("field out of uint32 range: ") + field_name;
                    lua_pop(state, 1);
                    return false;
                }

                value = static_cast<std::uint32_t>(raw);

                lua_pop(state, 1);
                return true;
            }

            [[nodiscard]] auto parse_kind(
                std::string_view text,
                core::item::Kind& kind) -> bool
            {
                if (text == "equipment")
                {
                    kind = core::item::Kind::equipment;
                    return true;
                }

                if (text == "consumable")
                {
                    kind = core::item::Kind::consumable;
                    return true;
                }

                if (text == "material")
                {
                    kind = core::item::Kind::material;
                    return true;
                }

                if (text == "quest")
                {
                    kind = core::item::Kind::quest;
                    return true;
                }

                if (text == "collectible")
                {
                    kind = core::item::Kind::collectible;
                    return true;
                }

                return false;
            }

            [[nodiscard]] auto parse_equipment_class(
                std::string_view text,
                core::item::EquipmentClass& equipment_class) -> bool
            {
                if (text == "weapon")
                {
                    equipment_class = core::item::EquipmentClass::weapon;
                    return true;
                }

                if (text == "armor")
                {
                    equipment_class = core::item::EquipmentClass::armor;
                    return true;
                }

                if (text == "accessory")
                {
                    equipment_class = core::item::EquipmentClass::accessory;
                    return true;
                }

                if (text == "tool")
                {
                    equipment_class = core::item::EquipmentClass::tool;
                    return true;
                }

                return false;
            }

            [[nodiscard]] auto parse_equipment_slot(
                std::string_view text,
                core::item::EquipmentSlot& slot) -> bool
            {
                if (text == "head")
                {
                    slot = core::item::EquipmentSlot::head;
                    return true;
                }

                if (text == "chest")
                {
                    slot = core::item::EquipmentSlot::chest;
                    return true;
                }

                if (text == "hands")
                {
                    slot = core::item::EquipmentSlot::hands;
                    return true;
                }

                if (text == "legs")
                {
                    slot = core::item::EquipmentSlot::legs;
                    return true;
                }

                if (text == "feet")
                {
                    slot = core::item::EquipmentSlot::feet;
                    return true;
                }

                if (text == "main_hand")
                {
                    slot = core::item::EquipmentSlot::main_hand;
                    return true;
                }

                if (text == "off_hand")
                {
                    slot = core::item::EquipmentSlot::off_hand;
                    return true;
                }

                if (text == "ring_1")
                {
                    slot = core::item::EquipmentSlot::ring_1;
                    return true;
                }

                if (text == "ring_2")
                {
                    slot = core::item::EquipmentSlot::ring_2;
                    return true;
                }

                if (text == "earring_1")
                {
                    slot = core::item::EquipmentSlot::earring_1;
                    return true;
                }

                if (text == "earring_2")
                {
                    slot = core::item::EquipmentSlot::earring_2;
                    return true;
                }

                if (text == "necklace")
                {
                    slot = core::item::EquipmentSlot::necklace;
                    return true;
                }

                if (text == "tool")
                {
                    slot = core::item::EquipmentSlot::tool;
                    return true;
                }

                return false;
            }

            [[nodiscard]] auto read_primary(
                lua_State* state,
                int table_index,
                const LuaLoadOptions& options,
                core::stat::Primary& primary,
                std::string& error) -> bool
            {
                return
                    read_int32_field(state, table_index, "vitality", primary.vitality, options, error) &&
                    read_int32_field(state, table_index, "strength", primary.strength, options, error) &&
                    read_int32_field(state, table_index, "agility", primary.agility, options, error) &&
                    read_int32_field(state, table_index, "intellect", primary.intellect, options, error) &&
                    read_int32_field(state, table_index, "faith", primary.faith, options, error) &&
                    read_int32_field(state, table_index, "dexterity", primary.dexterity, options, error) &&
                    read_int32_field(state, table_index, "luck", primary.luck, options, error);
            }

            [[nodiscard]] auto parse_equipment(
                lua_State* state,
                int item_table_index,
                const LuaLoadOptions& options,
                core::item::EquipmentDefinition& equipment,
                std::string& error) -> bool
            {
                lua_getfield(state, item_table_index, "equipment");

                if (!lua_istable(state, -1))
                {
                    lua_pop(state, 1);

                    if (strict_mode(options) && options.require_equipment_table_for_equipment)
                    {
                        error = "equipment table is required for equipment item";
                        return false;
                    }

                    return true;
                }

                const int equipment_table_index = lua_gettop(state);

                std::string class_text;

                if (read_string_field(state, equipment_table_index, "class", class_text))
                {
                    if (!parse_equipment_class(class_text, equipment.equipment_class) && strict_mode(options))
                    {
                        error = "unknown equipment class: " + class_text;
                        lua_pop(state, 1);
                        return false;
                    }
                }
                else if (strict_mode(options))
                {
                    error = "equipment class is required";
                    lua_pop(state, 1);
                    return false;
                }

                std::string slot_text;

                if (read_string_field(state, equipment_table_index, "slot", slot_text))
                {
                    if (!parse_equipment_slot(slot_text, equipment.slot) && strict_mode(options))
                    {
                        error = "unknown equipment slot: " + slot_text;
                        lua_pop(state, 1);
                        return false;
                    }
                }
                else if (strict_mode(options))
                {
                    error = "equipment slot is required";
                    lua_pop(state, 1);
                    return false;
                }

                if (!read_int32_field(state, equipment_table_index, "attack", equipment.attack, options, error) ||
                    !read_int32_field(state, equipment_table_index, "defense", equipment.defense, options, error) ||
                    !read_uint32_field(state, equipment_table_index, "weight", equipment.weight, options, error) ||
                    !read_uint32_field(state, equipment_table_index, "max_durability", equipment.max_durability, options, error) ||
                    !read_uint32_field(state, equipment_table_index, "modification_slots", equipment.modification_slots, options, error) ||
                    !read_uint32_field(state, equipment_table_index, "affix_slots", equipment.affix_slots, options, error))
                {
                    lua_pop(state, 1);
                    return false;
                }

                read_bool_field(state, equipment_table_index, "two_handed", equipment.two_handed);

                lua_getfield(state, equipment_table_index, "requirements");

                if (lua_istable(state, -1))
                {
                    const int requirements_table_index = lua_gettop(state);

                    lua_getfield(state, requirements_table_index, "minimum_primary");

                    if (lua_istable(state, -1))
                    {
                        const int minimum_primary_table_index = lua_gettop(state);

                        if (!read_primary(
                                state,
                                minimum_primary_table_index,
                                options,
                                equipment.requirements.minimum_primary,
                                error))
                        {
                            lua_pop(state, 2);
                            return false;
                        }
                    }

                    lua_pop(state, 1);

                    if (!read_int32_field(
                            state,
                            requirements_table_index,
                            "under_requirement_penalty_percent",
                            equipment.requirements.under_requirement_penalty_percent,
                            options,
                            error))
                    {
                        lua_pop(state, 2);
                        return false;
                    }
                }

                lua_pop(state, 1);

                lua_pop(state, 1);
                return true;
            }

            auto increment_kind_stats(core::item::Kind kind, LuaLoadStats& stats) -> void
            {
                switch (kind)
                {
                    case core::item::Kind::equipment:
                        ++stats.equipment_loaded;
                        break;

                    case core::item::Kind::material:
                        ++stats.materials_loaded;
                        break;

                    case core::item::Kind::consumable:
                        ++stats.consumables_loaded;
                        break;

                    case core::item::Kind::quest:
                        ++stats.quest_items_loaded;
                        break;

                    case core::item::Kind::collectible:
                        ++stats.collectibles_loaded;
                        break;
                }
            }

            [[nodiscard]] auto select_items_table(
                lua_State* state,
                const LuaLoadOptions& options,
                std::string& error) -> bool
            {
                const int top = lua_gettop(state);

                if (options.allow_returned_table && top >= 1 && lua_istable(state, -1))
                {
                    return true;
                }

                if (options.allow_global_items_table)
                {
                    lua_getglobal(state, "items");

                    if (lua_istable(state, -1))
                    {
                        return true;
                    }

                    lua_pop(state, 1);
                }

                if (!options.allow_returned_table && !options.allow_global_items_table)
                {
                    error = "no Lua table source enabled";
                    return false;
                }

                error = "lua script did not return a table and global 'items' was not found";
                return false;
            }

            [[nodiscard]] auto parse_item(
                lua_State* state,
                lua_Integer key,
                int item_table_index,
                const LuaLoadOptions& options,
                ParsedItem& parsed,
                std::string& error) -> bool
            {
                if (key <= 0)
                {
                    error = "item id must be positive";
                    return false;
                }

                if (static_cast<unsigned long long>(key) > std::numeric_limits<std::uint32_t>::max())
                {
                    error = "item id out of supported range";
                    return false;
                }

                parsed.definition.identity.item_template_id =
                    static_cast<core::id::ItemTemplateId>(static_cast<std::uint32_t>(key));

                if (read_string_field(state, item_table_index, "name", parsed.name))
                {
                    if (parsed.name.empty() && strict_mode(options) && options.require_name)
                    {
                        error = "item name cannot be empty";
                        return false;
                    }
                }
                else if (strict_mode(options) && options.require_name)
                {
                    error = "item name is required";
                    return false;
                }

                std::string kind_text;
                bool kind_was_read = false;

                if (read_string_field(state, item_table_index, "kind", kind_text))
                {
                    kind_was_read = true;

                    core::item::Kind parsed_kind{};

                    if (!parse_kind(kind_text, parsed_kind))
                    {
                        if (strict_mode(options))
                        {
                            error = "unknown item kind: " + kind_text;
                            return false;
                        }
                    }
                    else
                    {
                        parsed.definition.kind = parsed_kind;
                    }
                }
                else if (strict_mode(options) && options.require_kind)
                {
                    error = "item kind is required";
                    return false;
                }

                if (kind_was_read && parsed.definition.kind == core::item::Kind::equipment)
                {
                    core::item::EquipmentDefinition equipment{};

                    if (!parse_equipment(
                            state,
                            item_table_index,
                            options,
                            equipment,
                            error))
                    {
                        return false;
                    }

                    parsed.definition.equipment = equipment;
                }

                return true;
            }
        }

        LuaLoadResult load_items_from_lua_ex(
            const std::string& path,
            core::item::Catalog& catalog,
            const LuaLoadOptions& options)
        {
            LuaLoadStats stats{};

            lua_State* state = luaL_newstate();

            if (state == nullptr)
            {
                return make_error("failed to create lua state", stats);
            }

            luaL_openlibs(state);

            if (luaL_dofile(state, path.c_str()) != LUA_OK)
            {
                const char* message = lua_tostring(state, -1);
                const std::string error = message != nullptr
                    ? message
                    : "unknown error running lua file";

                lua_close(state);
                return make_error(error, stats);
            }

            std::string error;

            if (!select_items_table(state, options, error))
            {
                lua_close(state);
                return make_error(error, stats);
            }

            const int items_table_index = lua_gettop(state);

            std::vector<ParsedItem> parsed_items;
            parsed_items.reserve(128);

            std::unordered_set<std::uint32_t> ids_seen_in_file;

            lua_pushnil(state);

            while (lua_next(state, items_table_index) != 0)
            {
                if (!lua_isinteger(state, -2))
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error("item key is not an integer", stats);
                }

                const lua_Integer key = lua_tointeger(state, -2);

                if (!lua_istable(state, -1))
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error(
                        error_for_item(key, "item entry is not a table"),
                        stats);
                }

                if (key <= 0 ||
                    static_cast<unsigned long long>(key) > std::numeric_limits<std::uint32_t>::max())
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error(
                        error_for_item(key, "item id out of supported range"),
                        stats);
                }

                const auto normalized_id = static_cast<std::uint32_t>(key);

                if (!ids_seen_in_file.insert(normalized_id).second)
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error(
                        error_for_item(key, "duplicated item id in file"),
                        stats);
                }

                if (catalog.find(static_cast<core::id::ItemTemplateId>(normalized_id)) != nullptr)
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error(
                        error_for_item(key, "item id already exists in destination catalog"),
                        stats);
                }

                ++stats.items_seen;

                ParsedItem parsed{};

                if (!parse_item(
                        state,
                        key,
                        lua_gettop(state),
                        options,
                        parsed,
                        error))
                {
                    lua_pop(state, 1);
                    lua_close(state);
                    return make_error(error_for_item(key, error), stats);
                }

                parsed_items.push_back(std::move(parsed));

                lua_pop(state, 1);
            }

            lua_close(state);

            // Commit phase.
            //
            // The catalog is only modified after the full Lua table has been parsed
            // and validated. This prevents parse-time partial pollution.
            for (auto& parsed : parsed_items)
            {
                if (!parsed.name.empty())
                {
                    s_persistent_strings.push_back(parsed.name);
                    parsed.definition.identity.name = s_persistent_strings.back();
                }

                catalog.insert(parsed.definition);

                ++stats.items_loaded;
                increment_kind_stats(parsed.definition.kind, stats);
            }

            return make_success(stats);
        }

        bool load_items_from_lua(
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
    }

#endif // MMO_USE_LUA

