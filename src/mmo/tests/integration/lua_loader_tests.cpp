#include "mmo/persistence/lua_loader.hpp"
#include "mmo/tests/support/test.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace test = mmo::tests::support;

    class FixtureDirectory
    {
    public:
        explicit FixtureDirectory(std::string_view name)
            : path_(std::filesystem::path{ MMO_TEST_BINARY_DIR } / "test_tmp" / "lua_loader" / name)
        {
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
        }

        ~FixtureDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        FixtureDirectory(const FixtureDirectory&) = delete;
        auto operator=(const FixtureDirectory&) -> FixtureDirectory& = delete;

        [[nodiscard]] auto write(std::string_view file_name, std::string_view content) const
            -> std::filesystem::path
        {
            const auto file_path = path_ / file_name;
            std::ofstream file(file_path, std::ios::binary);
            test::require(file.good(), "failed to open Lua fixture");
            file << content;
            test::require(file.good(), "failed to write Lua fixture");
            return file_path;
        }

    private:
        std::filesystem::path path_;
    };

    [[nodiscard]] auto make_existing_item(
        mmo::core::id::ItemTemplateId id,
        std::string name) -> mmo::core::item::Definition
    {
        auto definition = mmo::core::item::make_material_definition();
        definition.identity.item_template_id = id;
        definition.identity.name = std::move(name);
        return definition;
    }

    auto require_failed_load(
        const mmo::persistence::LuaLoadResult& result,
        const mmo::core::item::Catalog& catalog,
        std::size_t expected_size,
        std::string_view label) -> void
    {
        test::require(!result.ok, std::string(label).append(" should fail"));
        test::require(!result.error.empty(), std::string(label).append(" should report an error"));
        test::require_equal(expected_size, catalog.size(), std::string(label).append(" catalog size"));
    }

    auto test_repository_content(std::string& details) -> void
    {
        const auto items_path =
            std::filesystem::path{ MMO_TEST_SOURCE_DIR } / "data" / "lua" / "tables" / "items.lua";
        mmo::core::item::Catalog catalog{};

        const auto result = mmo::persistence::load_items_from_lua_ex(
            items_path.string(),
            catalog);

        test::require(result.ok, result.error.empty() ? "repository content load failed" : result.error);
        test::require_equal(static_cast<std::uint32_t>(3), result.stats.items_seen, "items seen");
        test::require_equal(static_cast<std::uint32_t>(3), result.stats.items_loaded, "items loaded");
        test::require_equal(static_cast<std::uint32_t>(2), result.stats.equipment_loaded, "equipment loaded");
        test::require_equal(static_cast<std::uint32_t>(1), result.stats.consumables_loaded, "consumables loaded");
        test::require_equal(static_cast<std::size_t>(3), catalog.size(), "catalog size");

        const auto* sword = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1001));
        test::require(sword != nullptr, "Short Sword should exist");
        test::require_equal(std::string("Short Sword"), sword->identity.name, "Short Sword name");
        test::require_equal(mmo::core::item::Kind::equipment, sword->kind, "Short Sword kind");
        test::require(sword->equipment.has_value(), "Short Sword equipment data");

        const auto& sword_equipment = sword->equipment.value();
        test::require_equal(mmo::core::item::EquipmentClass::weapon, sword_equipment.equipment_class, "Short Sword class");
        test::require_equal(mmo::core::item::EquipmentSlot::main_hand, sword_equipment.slot, "Short Sword slot");
        test::require_equal(static_cast<std::int32_t>(12), sword_equipment.attack, "Short Sword attack");
        test::require_equal(static_cast<std::int32_t>(0), sword_equipment.defense, "Short Sword defense");
        test::require_equal(static_cast<std::uint32_t>(8), sword_equipment.weight, "Short Sword weight");
        test::require(!sword_equipment.two_handed, "Short Sword should be one-handed");
        test::require_equal(static_cast<std::uint32_t>(10), sword_equipment.max_durability, "Short Sword durability");
        test::require_equal(static_cast<std::uint32_t>(2), sword_equipment.modification_slots, "Short Sword modification slots");
        test::require_equal(static_cast<std::uint32_t>(3), sword_equipment.affix_slots, "Short Sword affix slots");
        test::require_equal(static_cast<std::int32_t>(10), sword_equipment.requirements.minimum_primary.strength, "Short Sword strength");
        test::require_equal(static_cast<std::int32_t>(6), sword_equipment.requirements.minimum_primary.dexterity, "Short Sword dexterity");
        test::require_equal(static_cast<std::int32_t>(25), sword_equipment.requirements.under_requirement_penalty_percent, "Short Sword penalty");

        const auto* ring = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1002));
        test::require(ring != nullptr && ring->equipment.has_value(), "Iron Ring should exist");
        test::require_equal(std::string("Iron Ring"), ring->identity.name, "Iron Ring name");
        test::require_equal(mmo::core::item::Kind::equipment, ring->kind, "Iron Ring kind");
        test::require_equal(mmo::core::item::EquipmentClass::accessory, ring->equipment->equipment_class, "Iron Ring class");
        test::require_equal(mmo::core::item::EquipmentSlot::ring_1, ring->equipment->slot, "Iron Ring slot");
        test::require_equal(static_cast<std::int32_t>(0), ring->equipment->attack, "Iron Ring attack");
        test::require_equal(static_cast<std::int32_t>(0), ring->equipment->defense, "Iron Ring defense");
        test::require_equal(static_cast<std::uint32_t>(1), ring->equipment->weight, "Iron Ring weight");
        test::require(!ring->equipment->two_handed, "Iron Ring should not be two-handed");
        test::require_equal(static_cast<std::uint32_t>(15), ring->equipment->max_durability, "Iron Ring durability");
        test::require_equal(static_cast<std::uint32_t>(1), ring->equipment->modification_slots, "Iron Ring modification slots");
        test::require_equal(static_cast<std::uint32_t>(1), ring->equipment->affix_slots, "Iron Ring affix slots");
        test::require_equal(static_cast<std::int32_t>(5), ring->equipment->requirements.minimum_primary.faith, "Iron Ring faith");
        test::require_equal(static_cast<std::int32_t>(10), ring->equipment->requirements.under_requirement_penalty_percent, "Iron Ring penalty");

        const auto* potion = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(2001));
        test::require(potion != nullptr, "Health Potion should exist");
        test::require_equal(mmo::core::item::Kind::consumable, potion->kind, "Health Potion kind");

        details = "items=3 sword=1001 ring=1002 potion=2001";
    }

    auto test_owned_string_lifetime(std::string& details) -> void
    {
        FixtureDirectory fixture{ "owned_lifetime" };
        const auto path = fixture.write(
            "owned.lua",
            R"lua(return { [3101] = { name = "VM-owned source text", kind = "material" } })lua");
        mmo::core::item::Catalog catalog{};

        {
            const auto result = mmo::persistence::load_items_from_lua_ex(path.string(), catalog);
            test::require(result.ok, result.error.empty() ? "owned string load failed" : result.error);
        }

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(3101));
        test::require(definition != nullptr, "owned item should exist after loader returns");
        test::require_equal(
            std::string("VM-owned source text"),
            definition->identity.name,
            "catalog-owned name after lua_close");
        details = "lua_state_closed name_still_owned=true";
    }

    auto test_syntax_error_is_atomic(std::string& details) -> void
    {
        FixtureDirectory fixture{ "syntax_error" };
        const auto path = fixture.write(
            "invalid.lua",
            "return { [3201] = { name = 'Broken', kind = 'material' } ");
        mmo::core::item::Catalog catalog{};
        test::require(catalog.insert(make_existing_item(
            static_cast<mmo::core::id::ItemTemplateId>(9000), "Existing")), "seed insert");

        const auto result = mmo::persistence::load_items_from_lua_ex(path.string(), catalog);

        require_failed_load(result, catalog, 1, "syntax error");
        test::require(catalog.find(static_cast<mmo::core::id::ItemTemplateId>(3201)) == nullptr, "syntax item should not publish");
        details = "catalog_before=1 catalog_after=1";
    }

    auto test_strict_validation_matrix(std::string& details) -> void
    {
        struct InvalidCase
        {
            std::string_view name;
            std::string_view script;
        };

        const std::vector<InvalidCase> cases{
            { "missing_name", R"lua(return { [1] = { kind = "material" } })lua" },
            { "missing_kind", R"lua(return { [1] = { name = "Missing Kind" } })lua" },
            { "unknown_kind", R"lua(return { [1] = { name = "Unknown", kind = "mystery" } })lua" },
            { "missing_equipment", R"lua(return { [1] = { name = "Sword", kind = "equipment" } })lua" },
            { "unknown_class", R"lua(return { [1] = { name = "Sword", kind = "equipment", equipment = { class = "mystery", slot = "main_hand" } } })lua" },
            { "unknown_slot", R"lua(return { [1] = { name = "Sword", kind = "equipment", equipment = { class = "weapon", slot = "mystery" } } })lua" },
        };

        FixtureDirectory fixture{ "strict_validation" };
        mmo::persistence::LuaLoadOptions options{};
        options.mode = mmo::persistence::LuaLoadMode::strict;
        options.require_name = true;
        options.require_kind = true;
        options.require_equipment_table_for_equipment = true;

        for (const auto& invalid_case : cases)
        {
            const auto path = fixture.write(
                std::string(invalid_case.name).append(".lua"),
                invalid_case.script);
            mmo::core::item::Catalog catalog{};
            const auto result = mmo::persistence::load_items_from_lua_ex(
                path.string(), catalog, options);
            require_failed_load(result, catalog, 0, invalid_case.name);
        }

        details = "strict_rejections=6";
    }

    auto test_numeric_boundaries(std::string& details) -> void
    {
        struct InvalidCase
        {
            std::string_view name;
            std::string_view script;
        };

        const std::vector<InvalidCase> cases{
            { "negative_unsigned", R"lua(return { [1] = { name = "Sword", kind = "equipment", equipment = { class = "weapon", slot = "main_hand", weight = -1 } } })lua" },
            { "uint32_overflow", R"lua(return { [1] = { name = "Sword", kind = "equipment", equipment = { class = "weapon", slot = "main_hand", weight = 4294967296 } } })lua" },
            { "int32_overflow", R"lua(return { [1] = { name = "Sword", kind = "equipment", equipment = { class = "weapon", slot = "main_hand", attack = 2147483648 } } })lua" },
        };

        FixtureDirectory fixture{ "numeric_boundaries" };

        for (const auto& invalid_case : cases)
        {
            const auto path = fixture.write(
                std::string(invalid_case.name).append(".lua"),
                invalid_case.script);
            mmo::core::item::Catalog catalog{};
            const auto result = mmo::persistence::load_items_from_lua_ex(path.string(), catalog);
            require_failed_load(result, catalog, 0, invalid_case.name);
        }

        details = "numeric_rejections=3";
    }

    auto test_atomic_load_preserves_previous_catalog(std::string& details) -> void
    {
        FixtureDirectory fixture{ "atomic_load" };
        const auto path = fixture.write(
            "atomic.lua",
            R"lua(return {
                [9001] = { name = "Valid New Item", kind = "material" },
                [9002] = { name = "Invalid New Item", kind = "unknown" },
            })lua");
        mmo::core::item::Catalog catalog{};
        test::require(catalog.insert(make_existing_item(
            static_cast<mmo::core::id::ItemTemplateId>(9000), "Existing Item")), "seed insert");
        mmo::persistence::LuaLoadOptions options{};
        options.atomic = true;

        const auto result = mmo::persistence::load_items_from_lua_ex(
            path.string(), catalog, options);

        require_failed_load(result, catalog, 1, "atomic load");
        test::require(catalog.find(static_cast<mmo::core::id::ItemTemplateId>(9000)) != nullptr, "existing item retained");
        test::require(catalog.find(static_cast<mmo::core::id::ItemTemplateId>(9001)) == nullptr, "valid staged item not published");
        test::require(catalog.find(static_cast<mmo::core::id::ItemTemplateId>(9002)) == nullptr, "invalid staged item not published");
        details = "before={9000} after={9000}";
    }

    auto test_existing_id_collision(std::string& details) -> void
    {
        FixtureDirectory fixture{ "id_collision" };
        const auto path = fixture.write(
            "collision.lua",
            R"lua(return { [9000] = { name = "Replacement", kind = "material" } })lua");
        mmo::core::item::Catalog catalog{};
        test::require(catalog.insert(make_existing_item(
            static_cast<mmo::core::id::ItemTemplateId>(9000), "Original")), "seed insert");

        const auto result = mmo::persistence::load_items_from_lua_ex(path.string(), catalog);

        require_failed_load(result, catalog, 1, "existing id collision");
        const auto* original = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(9000));
        test::require(original != nullptr, "original item retained");
        test::require_equal(std::string("Original"), original->identity.name, "original item not overwritten");
        details = "collision_rejected=true original_retained=true";
    }

    auto test_returned_and_global_tables(std::string& details) -> void
    {
        FixtureDirectory fixture{ "table_sources" };
        const auto returned_path = fixture.write(
            "returned.lua",
            R"lua(return { [4101] = { name = "Returned", kind = "material" } })lua");
        const auto global_path = fixture.write(
            "global.lua",
            R"lua(items = { [4102] = { name = "Global", kind = "material" } })lua");

        mmo::persistence::LuaLoadOptions returned_only{};
        returned_only.allow_returned_table = true;
        returned_only.allow_global_items_table = false;
        mmo::core::item::Catalog returned_catalog{};
        const auto returned_result = mmo::persistence::load_items_from_lua_ex(
            returned_path.string(), returned_catalog, returned_only);
        test::require(returned_result.ok, "returned table should load");
        test::require(returned_catalog.find(static_cast<mmo::core::id::ItemTemplateId>(4101)) != nullptr, "returned item exists");

        mmo::persistence::LuaLoadOptions global_only{};
        global_only.allow_returned_table = false;
        global_only.allow_global_items_table = true;
        mmo::core::item::Catalog global_catalog{};
        const auto global_result = mmo::persistence::load_items_from_lua_ex(
            global_path.string(), global_catalog, global_only);
        test::require(global_result.ok, "global table should load");
        test::require(global_catalog.find(static_cast<mmo::core::id::ItemTemplateId>(4102)) != nullptr, "global item exists");

        mmo::core::item::Catalog disabled_source_catalog{};
        const auto disabled_source_result = mmo::persistence::load_items_from_lua_ex(
            returned_path.string(), disabled_source_catalog, global_only);
        require_failed_load(disabled_source_result, disabled_source_catalog, 0, "disabled returned source");
        details = "returned=true global=true disabled_source_rejected=true";
    }

    auto test_permissive_ignores_invalid_optional_field(std::string& details) -> void
    {
        FixtureDirectory fixture{ "permissive" };
        const auto path = fixture.write(
            "optional_field.lua",
            R"lua(return { [4201] = {
                name = "Permissive Sword",
                kind = "equipment",
                equipment = { class = "weapon", slot = "main_hand", weight = "heavy" },
            } })lua");

        mmo::core::item::Catalog strict_catalog{};
        mmo::persistence::LuaLoadOptions strict_options{};
        strict_options.mode = mmo::persistence::LuaLoadMode::strict;
        const auto strict_result = mmo::persistence::load_items_from_lua_ex(
            path.string(), strict_catalog, strict_options);
        require_failed_load(strict_result, strict_catalog, 0, "strict optional type");

        mmo::core::item::Catalog permissive_catalog{};
        mmo::persistence::LuaLoadOptions permissive_options{};
        permissive_options.mode = mmo::persistence::LuaLoadMode::permissive;
        const auto permissive_result = mmo::persistence::load_items_from_lua_ex(
            path.string(), permissive_catalog, permissive_options);
        test::require(permissive_result.ok, "permissive optional type should load");

        const auto* definition = permissive_catalog.find(
            static_cast<mmo::core::id::ItemTemplateId>(4201));
        test::require(definition != nullptr && definition->equipment.has_value(), "permissive item should exist");
        test::require_equal(static_cast<std::uint32_t>(0), definition->equipment->weight, "invalid weight keeps default");
        details = "strict=failed permissive=loaded weight=0";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("integration.lua.repository_content", test_repository_content));
    results.push_back(test::run_test("integration.lua.owned_string_lifetime", test_owned_string_lifetime));
    results.push_back(test::run_test("integration.lua.syntax_error_is_atomic", test_syntax_error_is_atomic));
    results.push_back(test::run_test("integration.lua.strict_validation_matrix", test_strict_validation_matrix));
    results.push_back(test::run_test("integration.lua.numeric_boundaries", test_numeric_boundaries));
    results.push_back(test::run_test("integration.lua.atomic_load", test_atomic_load_preserves_previous_catalog));
    results.push_back(test::run_test("integration.lua.existing_id_collision", test_existing_id_collision));
    results.push_back(test::run_test("integration.lua.table_sources", test_returned_and_global_tables));
    results.push_back(test::run_test("integration.lua.permissive_optional_field", test_permissive_ignores_invalid_optional_field));
    return test::report_results("Lua integration", results, std::cout);
}
