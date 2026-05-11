#pragma once

#include <string>

#include "mmo/core/item.hpp"

namespace mmo
{
    namespace persistence
    {
#ifdef MMO_USE_LUA
        // Load items from a Lua table file. On success returns true and
        // populates the provided catalog. On failure returns false and fills
        // `out_error` with a human readable message.
        bool load_items_from_lua(const std::string& path, core::item::Catalog& catalog, std::string& out_error);
#else
        inline bool load_items_from_lua(const std::string& /*path*/, core::item::Catalog& /*catalog*/, std::string& out_error)
        {
            out_error = "Lua support disabled (MMO_USE_LUA not defined)";
            return false;
        }
#endif
    }
}
