#include "mmo/persistence/lua_loader.hpp"
#include "mmo/tests/support/test.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace test = mmo::tests::support;

    auto test_disabled_loader_preserves_catalog(std::string& details) -> void
    {
        mmo::core::item::Catalog catalog{};
        auto existing = mmo::core::item::make_material_definition();
        existing.identity.item_template_id = static_cast<mmo::core::id::ItemTemplateId>(9000);
        existing.identity.name = "Existing Item";

        test::require(catalog.insert(existing), "existing item should insert");

        const auto result = mmo::persistence::load_items_from_lua_ex(
            "lua-support-is-disabled.lua",
            catalog);

        test::require(!result.ok, "disabled Lua loader should fail");
        test::require_equal(
            std::string("Lua support disabled (MMO_USE_LUA not defined)"),
            result.error,
            "disabled Lua error");
        test::require_equal(static_cast<std::size_t>(1), catalog.size(), "catalog size");
        test::require(
            catalog.find(static_cast<mmo::core::id::ItemTemplateId>(9000)) != nullptr,
            "existing item should remain");

        details = "catalog_before=1 catalog_after=1";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test(
        "integration.lua.disabled_preserves_catalog",
        test_disabled_loader_preserves_catalog));
    return test::report_results("Lua disabled integration", results, std::cout);
}
