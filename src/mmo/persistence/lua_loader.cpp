#include "mmo/persistence/lua_loader.hpp"
#ifdef MMO_USE_LUA
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <vector>
#include <string>
#include <sstream>

namespace mmo
{
    namespace persistence
    {
        // Keep string copies so `std::string_view` stored in definitions
        // remains valid for the lifetime of the process. This simple pool
        // is acceptable for the loader skeleton; production code should use
        // a dedicated string pool or store std::string in definitions.
        static std::vector<std::string> s_persistent_strings;

        bool load_items_from_lua(const std::string& path, core::item::Catalog& catalog, std::string& out_error)
        {
            lua_State* L = luaL_newstate();
            if (!L)
            {
                out_error = "failed to create lua state";
                return false;
            }

            luaL_openlibs(L);

            if (luaL_dofile(L, path.c_str()) != LUA_OK)
            {
                const char* msg = lua_tostring(L, -1);
                out_error = msg ? msg : "unknown error running lua file";
                lua_close(L);
                return false;
            }

            // At this point the chunk may have returned a table, or the
            // script may have created a global `items` table. Prefer the
            // returned value if present; otherwise check global `items`.
            int top = lua_gettop(L);
            bool have_table = false;
            if (top >= 1 && lua_istable(L, -1))
            {
                have_table = true;
            }
            else
            {
                lua_getglobal(L, "items");
                if (lua_istable(L, -1))
                {
                    have_table = true;
                }
            }

            if (!have_table)
            {
                out_error = "lua script did not return a table and global 'items' not found";
                lua_close(L);
                return false;
            }

            // Iterate pairs: key => value
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                // key at -2, value at -1
                if (!lua_isinteger(L, -2))
                {
                    out_error = "item key is not an integer";
                    lua_settop(L, 0);
                    lua_close(L);
                    return false;
                }

                lua_Integer key = lua_tointeger(L, -2);

                core::item::Definition def{};
                def.identity.item_template_id = static_cast<core::id::ItemTemplateId>(key);

                if (!lua_istable(L, -1))
                {
                    std::ostringstream ss;
                    ss << "item entry is not a table for id: " << key;
                    out_error = ss.str();
                    lua_settop(L, 0);
                    lua_close(L);
                    return false;
                }

                // name (optional but useful)
                lua_getfield(L, -1, "name");
                if (lua_isstring(L, -1))
                {
                    const char* s = lua_tostring(L, -1);
                    s_persistent_strings.emplace_back(s ? s : "");
                    def.identity.name = s_persistent_strings.back();
                }
                lua_pop(L, 1);

                // kind (optional)
                lua_getfield(L, -1, "kind");
                if (lua_isstring(L, -1))
                {
                    const char* s = lua_tostring(L, -1);
                    if (s)
                    {
                        std::string kind(s);
                        if (kind == "equipment") def.kind = core::item::Kind::equipment;
                        else if (kind == "consumable") def.kind = core::item::Kind::consumable;
                        else if (kind == "material") def.kind = core::item::Kind::material;
                        else if (kind == "quest") def.kind = core::item::Kind::quest;
                        else if (kind == "collectible") def.kind = core::item::Kind::collectible;
                    }
                }
                lua_pop(L, 1);

                // equipment (best-effort parsing)
                if (def.kind == core::item::Kind::equipment)
                {
                    lua_getfield(L, -1, "equipment");
                    if (lua_istable(L, -1))
                    {
                        core::item::EquipmentDefinition edef{};
                        const int equipment_index = lua_gettop(L);

                        auto read_int32 = [L](int table_index, const char* field_name, std::int32_t& value) -> void {
                            lua_getfield(L, table_index, field_name);
                            if (lua_isinteger(L, -1))
                            {
                                value = static_cast<std::int32_t>(lua_tointeger(L, -1));
                            }
                            lua_pop(L, 1);
                        };

                        auto read_uint32 = [L](int table_index, const char* field_name, std::uint32_t& value) -> void {
                            lua_getfield(L, table_index, field_name);
                            if (lua_isinteger(L, -1))
                            {
                                value = static_cast<std::uint32_t>(lua_tointeger(L, -1));
                            }
                            lua_pop(L, 1);
                        };

                        auto read_primary = [&read_int32](int table_index) -> core::stat::Primary {
                            core::stat::Primary primary{};
                            read_int32(table_index, "vitality", primary.vitality);
                            read_int32(table_index, "strength", primary.strength);
                            read_int32(table_index, "agility", primary.agility);
                            read_int32(table_index, "intellect", primary.intellect);
                            read_int32(table_index, "faith", primary.faith);
                            read_int32(table_index, "dexterity", primary.dexterity);
                            read_int32(table_index, "luck", primary.luck);
                            return primary;
                        };

                        // class
                        lua_getfield(L, equipment_index, "class");
                        if (lua_isstring(L, -1))
                        {
                            std::string cls(lua_tostring(L, -1));
                            if (cls == "weapon") edef.equipment_class = core::item::EquipmentClass::weapon;
                            else if (cls == "armor") edef.equipment_class = core::item::EquipmentClass::armor;
                            else if (cls == "accessory") edef.equipment_class = core::item::EquipmentClass::accessory;
                            else if (cls == "tool") edef.equipment_class = core::item::EquipmentClass::tool;
                        }
                        lua_pop(L, 1);

                        // slot
                        lua_getfield(L, equipment_index, "slot");
                        if (lua_isstring(L, -1))
                        {
                            std::string slot(lua_tostring(L, -1));
                            if (slot == "head") edef.slot = core::item::EquipmentSlot::head;
                            else if (slot == "chest") edef.slot = core::item::EquipmentSlot::chest;
                            else if (slot == "hands") edef.slot = core::item::EquipmentSlot::hands;
                            else if (slot == "legs") edef.slot = core::item::EquipmentSlot::legs;
                            else if (slot == "feet") edef.slot = core::item::EquipmentSlot::feet;
                            else if (slot == "main_hand") edef.slot = core::item::EquipmentSlot::main_hand;
                            else if (slot == "off_hand") edef.slot = core::item::EquipmentSlot::off_hand;
                            else if (slot == "ring_1") edef.slot = core::item::EquipmentSlot::ring_1;
                            else if (slot == "ring_2") edef.slot = core::item::EquipmentSlot::ring_2;
                            else if (slot == "earring_1") edef.slot = core::item::EquipmentSlot::earring_1;
                            else if (slot == "earring_2") edef.slot = core::item::EquipmentSlot::earring_2;
                            else if (slot == "necklace") edef.slot = core::item::EquipmentSlot::necklace;
                            else if (slot == "tool") edef.slot = core::item::EquipmentSlot::tool;
                        }
                        lua_pop(L, 1);

                        read_int32(equipment_index, "attack", edef.attack);
                        read_int32(equipment_index, "defense", edef.defense);
                        read_uint32(equipment_index, "weight", edef.weight);

                        // two_handed
                        lua_getfield(L, equipment_index, "two_handed");
                        if (lua_isboolean(L, -1)) edef.two_handed = (lua_toboolean(L, -1) != 0);
                        lua_pop(L, 1);

                        // max_durability
                        lua_getfield(L, equipment_index, "max_durability");
                        if (lua_isinteger(L, -1)) edef.max_durability = static_cast<std::uint32_t>(lua_tointeger(L, -1));
                        lua_pop(L, 1);

                        read_uint32(equipment_index, "modification_slots", edef.modification_slots);
                        read_uint32(equipment_index, "affix_slots", edef.affix_slots);

                        lua_getfield(L, equipment_index, "requirements");
                        if (lua_istable(L, -1))
                        {
                            const int requirements_index = lua_gettop(L);

                            lua_getfield(L, requirements_index, "minimum_primary");
                            if (lua_istable(L, -1))
                            {
                                const int minimum_primary_index = lua_gettop(L);
                                edef.requirements.minimum_primary = read_primary(minimum_primary_index);
                            }
                            lua_pop(L, 1);

                            read_int32(requirements_index, "under_requirement_penalty_percent", edef.requirements.under_requirement_penalty_percent);
                        }
                        lua_pop(L, 1);

                        def.equipment = edef;
                    }
                    lua_pop(L, 1); // pop equipment
                }

                // best-effort insert
                catalog.insert(def);

                // pop value, keep key for next()
                lua_pop(L, 1);
            }

            lua_settop(L, 0);
            lua_close(L);
            return true;
        }
    }
}

#endif // MMO_USE_LUA
