#include "mmo/mmo.hpp"
#include "mmo/server/loop.hpp"
#include "mmo/core/inventory.hpp"
#include "mmo/core/damage.hpp"
#include "mmo/core/event.hpp"
#include "mmo/core/material.hpp"
#include "mmo/tests/support/test.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    using mmo::tests::support::format_test_summary_line;
    using mmo::tests::support::require;
    using mmo::tests::support::require_at_least;
    using mmo::tests::support::require_at_most;
    using mmo::tests::support::require_equal;
    using mmo::tests::support::TestResult;

    enum class StressLevel
    {
        smoke,
        normal,
        high,
        extreme
    };

    struct StressConfig
    {
        StressLevel level{ StressLevel::normal };

        std::uint32_t inventory_items_per_entity{ 256 };
        std::uint32_t inventory_iterations{ 5000 };

        std::uint32_t affix_count{ 5000 };
        std::uint32_t max_tier{ 5 };

        std::uint32_t max_level{ 120 };

        std::uint32_t entity_spawn_count{ 1000 };

        std::uint32_t world_entity_count{ 1000 };
        std::uint32_t world_items_per_entity{ 32 };
        std::uint32_t world_tick_rate{ 40 };
        std::uint32_t world_max_ticks{ 120 };

        std::uint32_t event_count{ 5000 };

        std::uint32_t combat_entity_count{ 1000 };
        std::uint32_t combat_sources_per_entity{ 8 };
        std::uint32_t combat_hits_per_source{ 4 };

        std::uint32_t material_damage_iterations{ 10000 };
    };

    auto read_environment_variable(const char* name) -> std::optional<std::string>
    {
#if defined(_MSC_VER)
        char* buffer = nullptr;
        std::size_t buffer_size = 0;

        if (_dupenv_s(&buffer, &buffer_size, name) != 0 || buffer == nullptr)
        {
            return std::nullopt;
        }

        std::string value{ buffer };
        std::free(buffer);
        return value;
#else
        const char* value = std::getenv(name);

        if (value == nullptr)
        {
            return std::nullopt;
        }

        return std::string{ value };
#endif
    }

    auto parse_stress_level() -> StressLevel
    {
        const auto level = read_environment_variable("MMO_STRESS_LEVEL");

        if (!level.has_value())
        {
            return StressLevel::normal;
        }

        if (*level == "smoke")
        {
            return StressLevel::smoke;
        }

        if (*level == "high")
        {
            return StressLevel::high;
        }

        if (*level == "extreme")
        {
            return StressLevel::extreme;
        }

        return StressLevel::normal;
    }

    auto stress_level_name(StressLevel level) -> std::string_view
    {
        switch (level)
        {
            case StressLevel::smoke:
                return "smoke";

            case StressLevel::normal:
                return "normal";

            case StressLevel::high:
                return "high";

            case StressLevel::extreme:
                return "extreme";
        }

        return "normal";
    }

    auto read_env_u64(const char* name, std::uint64_t fallback) -> std::uint64_t
    {
        const auto value = read_environment_variable(name);

        if (!value.has_value())
        {
            return fallback;
        }

        try
        {
            return static_cast<std::uint64_t>(std::stoull(*value));
        }
        catch (...)
        {
            return fallback;
        }
    }

    auto make_stress_config() -> StressConfig
    {
        StressConfig config{};
        config.level = parse_stress_level();

        switch (config.level)
        {
            case StressLevel::smoke:
                config.inventory_items_per_entity = 32;
                config.inventory_iterations = 200;

                config.affix_count = 200;
                config.max_tier = 5;
                config.max_level = 40;

                config.entity_spawn_count = 100;

                config.world_entity_count = 100;
                config.world_items_per_entity = 8;
                config.world_tick_rate = 20;
                config.world_max_ticks = 20;

                config.event_count = 500;

                config.combat_entity_count = 100;
                config.combat_sources_per_entity = 4;
                config.combat_hits_per_source = 3;

                config.material_damage_iterations = 1000;
                break;

            case StressLevel::normal:
                config.inventory_items_per_entity = 256;
                config.inventory_iterations = 5000;

                config.affix_count = 5000;
                config.max_tier = 5;
                config.max_level = 120;

                config.entity_spawn_count = 1000;

                config.world_entity_count = 1000;
                config.world_items_per_entity = 32;
                config.world_tick_rate = 40;
                config.world_max_ticks = 120;

                config.event_count = 10000;

                config.combat_entity_count = 1000;
                config.combat_sources_per_entity = 8;
                config.combat_hits_per_source = 4;

                config.material_damage_iterations = 10000;
                break;

            case StressLevel::high:
                config.inventory_items_per_entity = 1024;
                config.inventory_iterations = 10000;

                config.affix_count = 25000;
                config.max_tier = 5;
                config.max_level = 200;

                config.entity_spawn_count = 5000;

                config.world_entity_count = 5000;
                config.world_items_per_entity = 64;
                config.world_tick_rate = 40;
                config.world_max_ticks = 240;

                config.event_count = 50000;

                config.combat_entity_count = 5000;
                config.combat_sources_per_entity = 12;
                config.combat_hits_per_source = 6;

                config.material_damage_iterations = 50000;
                break;

            case StressLevel::extreme:
                config.inventory_items_per_entity = 4096;
                config.inventory_iterations = 25000;

                config.affix_count = 100000;
                config.max_tier = 5;
                config.max_level = 300;

                config.entity_spawn_count = 20000;

                config.world_entity_count = 20000;
                config.world_items_per_entity = 96;
                config.world_tick_rate = 40;
                config.world_max_ticks = 500;

                config.event_count = 200000;

                config.combat_entity_count = 20000;
                config.combat_sources_per_entity = 16;
                config.combat_hits_per_source = 8;

                config.material_damage_iterations = 200000;
                break;
        }

        return config;
    }

    template <typename WorldT>
    auto world_event_scheduler(WorldT& world) -> decltype(auto)
    {
        return (world.scheduler);
    }


    template <typename Fn>
    auto run_test(std::string_view name, mmo::core::log::Logger& logger, Fn&& fn) -> TestResult
    {
        logger.log(
            mmo::core::log::Level::info,
            "stress.test",
            std::string("BEGIN | ").append(std::string(name)));

        auto result = mmo::tests::support::run_test(name, std::forward<Fn>(fn));

        std::string end_message;
        end_message.append("END | ");
        end_message.append(std::string(name));
        end_message.append(" | ");
        end_message.append(result.ok ? "PASS" : "FAIL");
        end_message.append(" | duration=");
        end_message.append(std::to_string(result.duration_ms));
        end_message.append("ms");

        if (!result.details.empty())
        {
            end_message.append(" | ");
            end_message.append(result.details);
        }

        logger.log(
            result.ok ? mmo::core::log::Level::info : mmo::core::log::Level::error,
            "stress.test",
            end_message);

        return result;
    }

    auto format_summary_header(StressLevel level, std::size_t total, std::uint32_t failures) -> std::string
    {
        const auto passed = total >= static_cast<std::size_t>(failures)
            ? total - static_cast<std::size_t>(failures)
            : std::size_t{ 0 };

        std::string message;
        message.reserve(96);

        message.append("SUMMARY | level=");
        message.append(std::string(stress_level_name(level)));
        message.append(" | total=");
        message.append(std::to_string(total));
        message.append(" | passed=");
        message.append(std::to_string(passed));
        message.append(" | failed=");
        message.append(std::to_string(failures));

        return message;
    }

    auto build_item_catalog() -> mmo::core::item::Catalog
    {
        mmo::core::item::Catalog catalog{};

        for (std::uint32_t i = 1; i <= 64; ++i)
        {
            auto definition = mmo::core::item::make_equipment_definition(
                mmo::core::item::EquipmentClass::weapon,
                mmo::core::item::EquipmentSlot::main_hand);

            definition.identity.item_template_id = static_cast<mmo::core::id::ItemTemplateId>(i);
            definition.identity.name = "Stress Equipment";
            definition.equipment->attack = static_cast<std::int32_t>(5 + i);
            definition.equipment->weight = 2 + (i % 7);
            definition.equipment->max_durability = 100;

            catalog.insert(definition);
        }

        return catalog;
    }

    auto build_partial_item_catalog() -> mmo::core::item::Catalog
    {
        mmo::core::item::Catalog catalog{};

        for (std::uint32_t i = 1; i <= 64; i += 2)
        {
            auto definition = mmo::core::item::make_equipment_definition(
                mmo::core::item::EquipmentClass::weapon,
                mmo::core::item::EquipmentSlot::main_hand);

            definition.identity.item_template_id = static_cast<mmo::core::id::ItemTemplateId>(i);
            definition.identity.name = "Partial Stress Equipment";
            definition.equipment->attack = static_cast<std::int32_t>(5 + i);
            definition.equipment->weight = 2 + (i % 7);
            definition.equipment->max_durability = 100;

            catalog.insert(definition);
        }

        return catalog;
    }

    auto build_material_item_catalog() -> mmo::core::item::Catalog
    {
        auto catalog = build_item_catalog();

        auto material = mmo::core::item::make_material_definition();
        material.identity.item_template_id = static_cast<mmo::core::id::ItemTemplateId>(1001);
        material.identity.name = "Stress Material";
        material.stack.stackable = true;
        material.stack.max_stack = 999;

        catalog.insert(material);

        return catalog;
    }

    auto build_inventory(
        const mmo::core::item::Catalog& catalog,
        std::uint32_t items_per_entity,
        std::uint32_t requested_quantity,
        std::uint64_t item_id_base = 0) -> mmo::core::entity::Inventory
    {
        mmo::core::entity::Inventory inventory{};
        inventory.limits.max_slots = std::max<std::uint32_t>(items_per_entity, 100);
        inventory.items.reserve(items_per_entity);

        for (std::uint32_t i = 1; i <= items_per_entity; ++i)
        {
            const auto template_id = static_cast<mmo::core::id::ItemTemplateId>((i % 64) + 1);
            const auto* definition = catalog.find(template_id);

            if (definition == nullptr)
            {
                continue;
            }

            auto instance = mmo::core::item::make_instance(
                *definition,
                mmo::core::time::now(),
                requested_quantity);

            instance.item_id = static_cast<mmo::core::id::ItemId>(item_id_base + i);

            const auto add_result = mmo::core::inventory::add_item(catalog, inventory, instance);

            require(
                add_result.status == mmo::core::inventory::AddStatus::added,
                "failed to build equipment inventory using inventory contract");
        }

        return inventory;
    }

    auto expected_inventory_weight_from_instances(
        const mmo::core::item::Catalog& catalog,
        const mmo::core::entity::Inventory& inventory) -> std::uint64_t
    {
        std::uint64_t expected = 0;

        for (const auto& instance : inventory.items)
        {
            const auto* definition = catalog.find(instance.item_template_id);

            if (definition == nullptr || !definition->equipment.has_value())
            {
                continue;
            }

            expected +=
                static_cast<std::uint64_t>(definition->equipment->weight) *
                static_cast<std::uint64_t>(instance.quantity);
        }

        return expected;
    }

    void require_inventory_invariants(
        const mmo::core::item::Catalog& catalog,
        const mmo::core::entity::Inventory& inventory,
        std::string_view label)
    {
        const auto validation = mmo::core::inventory::validate(catalog, inventory);

        if (validation != mmo::core::inventory::ValidationIssue::none)
        {
            std::string error;
            error.append(std::string(label));
            error.append(" validation_issue=");
            error.append(std::to_string(static_cast<std::uint64_t>(validation)));

            throw std::runtime_error(error);
        }

        require(
            !mmo::core::inventory::has_duplicate_item_ids(inventory),
            "inventory invariant failed: duplicated item ids");
    }

    auto make_stress_blueprint(
        mmo::core::entity::Type type = mmo::core::entity::Type::player) -> mmo::core::entity::Blueprint
    {
        mmo::core::entity::Blueprint blueprint{};
        blueprint.template_id = 1;
        blueprint.species_id = 1;
        blueprint.type = type;
        blueprint.display_name = "Stress Entity";
        blueprint.base_primary = mmo::core::stat::Primary{ 12, 12, 8, 6, 5, 7, 4 };
        blueprint.level = 1;

        return blueprint;
    }

    auto is_allowed_experience_tier_reset(std::uint32_t level) -> bool
    {
        return level == 21 ||
               level == 41 ||
               level == 61 ||
               level == 81 ||
               level == 101 ||
               level == 121 ||
               level == 151 ||
               level == 201 ||
               level == 251;
    }

    auto make_map_material_definition() -> mmo::core::material::Definition
    {
        mmo::core::material::Definition definition{};

        definition.kind = mmo::core::material::Kind::stone;
        definition.name = "Stress Stone";
        definition.max_durability = 1000;
        definition.breakable = true;
        definition.ricochet_enabled = true;
        definition.entangle_enabled = false;

        definition.resistance.slash_resistance_percent = 40;
        definition.resistance.pierce_resistance_percent = 25;
        definition.resistance.sever_resistance_percent = 35;
        definition.resistance.impact_resistance_percent = 10;
        definition.resistance.magic_resistance_percent = 20;
        definition.resistance.elemental_resistance_percent = 50;

        return definition;
    }

    auto expected_material_damage(
        const mmo::core::material::Definition& definition,
        const mmo::core::damage::Packet& packet) -> std::int32_t
    {
        if (packet.amount <= 0)
        {
            return 0;
        }

        const auto resistance_percent =
            mmo::core::material::resistance_for(definition.resistance, packet.kind);

        const auto effective_percent =
            std::max<std::int32_t>(
                0,
                100 - resistance_percent + packet.penetration_percent);

        const auto resolved_damage =
            (static_cast<std::int64_t>(packet.amount) * effective_percent) / 100;

        return static_cast<std::int32_t>(
            std::max<std::int64_t>(resolved_damage, 0));
    }

    auto apply_combat_damage_to_entity(
        mmo::world::World& world,
        mmo::core::id::EntityId target_entity_id,
        const mmo::core::damage::Packet& packet,
        mmo::core::id::EntityId source_entity_id = mmo::core::id::invalid_entity_id) -> bool
    {
        if (packet.amount <= 0)
        {
            return true;
        }

        mmo::core::combat::DamageProfile profile{};
        profile.packet = packet;

        if (source_entity_id != mmo::core::id::invalid_entity_id)
        {
            profile.packet.source = mmo::core::damage::make_entity_source(source_entity_id);
        }

        return world.entities.apply_damage(
            target_entity_id,
            profile,
            mmo::core::time::now()).has_value();
    }

}
int main() {
    mmo::core::log::Logger logger{};
    logger.set_file("stress.log");

    const auto stress_config = make_stress_config();

    logger.log(
        mmo::core::log::Level::info,
        "stress",
        std::string("RUN | level=").append(std::string(stress_level_name(stress_config.level))));

    std::uint32_t failures = 0;
    std::vector<TestResult> results;

    auto push_result = [&](TestResult&& result) {
        failures += result.ok ? 0 : 1;
        results.push_back(std::move(result));
    };

    auto push_result_with_budget = [&](TestResult&& result, std::uint64_t max_duration_ms) {
        result.performance_budget_ms = max_duration_ms;
        result.budget_checked = true;

        if (result.ok && max_duration_ms > 0 && result.duration_ms > max_duration_ms)
        {
            result.ok = false;

            if (!result.details.empty())
            {
                result.details.append(" | ");
            }

            result.details.append("performance_budget_exceeded duration_ms=");
            result.details.append(std::to_string(result.duration_ms));
            result.details.append(" max_ms=");
            result.details.append(std::to_string(max_duration_ms));
        }

        failures += result.ok ? 0 : 1;
        results.push_back(std::move(result));
    };

    // =========================================================================
    // Inventory contract tests.
    // =========================================================================

    push_result(run_test("stress.item_catalog_owns_identity_strings", logger, [&](std::string& details) {
        mmo::core::item::Catalog catalog{};
        std::string source_name = "Owned Name";
        std::string source_description = "Owned Description";

        auto definition = mmo::core::item::make_material_definition();
        definition.identity.item_template_id =
            static_cast<mmo::core::id::ItemTemplateId>(9001);
        definition.identity.name = source_name;
        definition.identity.description = source_description;

        require(catalog.insert(definition), "owned identity definition should insert");

        source_name.assign("Mutated!!!");
        source_description.assign("Mutated Descript");
        definition.identity.name.clear();
        definition.identity.description.clear();

        const auto* stored = catalog.find(
            static_cast<mmo::core::id::ItemTemplateId>(9001));
        require(stored != nullptr, "owned identity definition should remain available");
        require_equal(std::string("Owned Name"), stored->identity.name, "catalog-owned item name");
        require_equal(
            std::string("Owned Description"),
            stored->identity.description,
            "catalog-owned item description");

        details.append("name_and_description_owned=true");
    }));

    push_result(run_test("stress.inventory_weight_exact", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        auto inventory = build_inventory(
            catalog,
            stress_config.inventory_items_per_entity,
            10,
            0);

        const auto expected_once = expected_inventory_weight_from_instances(catalog, inventory);
        const auto actual_once = mmo::core::inventory::calculate_weight(catalog, inventory);

        require_equal(
            expected_once,
            static_cast<std::uint64_t>(actual_once),
            "inventory weight exact");

        std::uint64_t total = 0;

        for (std::uint32_t i = 0; i < stress_config.inventory_iterations; ++i)
        {
            const auto current = mmo::core::inventory::calculate_weight(catalog, inventory);

            require_equal(
                expected_once,
                static_cast<std::uint64_t>(current),
                "inventory weight changed across iterations");

            total += static_cast<std::uint64_t>(current);
        }

        const auto expected_total =
            expected_once * static_cast<std::uint64_t>(stress_config.inventory_iterations);

        require_equal(expected_total, total, "inventory accumulated weight");

        details.append("expected_once=");
        details.append(std::to_string(expected_once));
        details.append(" actual_once=");
        details.append(std::to_string(static_cast<std::uint64_t>(actual_once)));
        details.append(" iterations=");
        details.append(std::to_string(stress_config.inventory_iterations));
        details.append(" total_weight_sum=");
        details.append(std::to_string(total));
    }));

    push_result(run_test("stress.inventory_empty", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        mmo::core::entity::Inventory inventory{};

        const auto actual = mmo::core::inventory::calculate_weight(catalog, inventory);

        require_equal(
            static_cast<std::uint64_t>(0),
            static_cast<std::uint64_t>(actual),
            "empty inventory weight");

        details.append("weight=");
        details.append(std::to_string(static_cast<std::uint64_t>(actual)));
    }));

    push_result(run_test("stress.inventory_missing_templates", logger, [&](std::string& details) {
        auto full_catalog = build_item_catalog();
        auto partial_catalog = build_partial_item_catalog();

        auto inventory = build_inventory(
            full_catalog,
            stress_config.inventory_items_per_entity,
            10,
            1000000);

        const auto expected = expected_inventory_weight_from_instances(partial_catalog, inventory);
        const auto actual = mmo::core::inventory::calculate_weight(partial_catalog, inventory);

        require_equal(
            expected,
            static_cast<std::uint64_t>(actual),
            "inventory weight with missing templates");

        details.append("expected_partial_weight=");
        details.append(std::to_string(expected));
        details.append(" actual_partial_weight=");
        details.append(std::to_string(static_cast<std::uint64_t>(actual)));
    }));

    push_result(run_test("stress.inventory_equipment_quantity_rule", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));
        require(definition != nullptr, "equipment template not found");

        auto instance = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            999);

        instance.item_id = static_cast<mmo::core::id::ItemId>(1);

        require_equal(
            static_cast<std::uint32_t>(1),
            instance.quantity,
            "equipment quantity must be forced to 1");

        mmo::core::entity::Inventory inventory{};

        const auto add_result = mmo::core::inventory::add_item(catalog, inventory, instance);

        require(
            add_result.status == mmo::core::inventory::AddStatus::added,
            "equipment instance was not added");

        require_equal(
            static_cast<std::size_t>(1),
            inventory.items.size(),
            "equipment slot count");

        require_equal(
            static_cast<std::uint32_t>(1),
            inventory.items.front().quantity,
            "equipment inventory quantity");

        details.append("equipment_quantity=");
        details.append(std::to_string(inventory.items.front().quantity));
    }));

    push_result(run_test("stress.inventory_duplicate_equipment_instances", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));
        require(definition != nullptr, "equipment template not found");

        mmo::core::entity::Inventory inventory{};
        inventory.limits.max_slots = 20;

        for (std::uint32_t i = 1; i <= 10; ++i)
        {
            auto instance = mmo::core::item::make_instance(
                *definition,
                mmo::core::time::now(),
                1);

            instance.item_id = static_cast<mmo::core::id::ItemId>(i);

            const auto add_result = mmo::core::inventory::add_item(catalog, inventory, instance);

            require(
                add_result.status == mmo::core::inventory::AddStatus::added,
                "duplicated equipment template was not added as separate instance");
        }

        require_equal(
            static_cast<std::size_t>(10),
            inventory.items.size(),
            "duplicate equipment instance count");

        require_equal(
            static_cast<std::uint32_t>(10),
            mmo::core::inventory::count_template_quantity(
                inventory,
                static_cast<mmo::core::id::ItemTemplateId>(1)),
            "duplicate equipment template quantity");

        require(
            !mmo::core::inventory::has_duplicate_item_ids(inventory),
            "duplicated item_id inside duplicate equipment test");

        const auto validation = mmo::core::inventory::validate(catalog, inventory);

        require(
            validation == mmo::core::inventory::ValidationIssue::none,
            "duplicate equipment inventory validation failed");

        details.append("equipment_instances=10 same_template=true unique_ids=true");
    }));

    push_result(run_test("stress.inventory_material_stack_rule", logger, [&](std::string& details) {
        auto catalog = build_material_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1001));
        require(definition != nullptr, "material template not found");

        mmo::core::entity::Inventory inventory{};

        auto first_stack = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            150);

        first_stack.item_id = static_cast<mmo::core::id::ItemId>(1001);

        auto second_stack = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            200);

        second_stack.item_id = static_cast<mmo::core::id::ItemId>(1002);

        const auto first_result = mmo::core::inventory::add_item(catalog, inventory, first_stack);

        require(
            first_result.status == mmo::core::inventory::AddStatus::added,
            "first material stack was not added");

        const auto second_result = mmo::core::inventory::add_item(catalog, inventory, second_stack);

        require(
            second_result.status == mmo::core::inventory::AddStatus::stacked,
            "second material stack was not merged into existing stack");

        require_equal(
            static_cast<std::size_t>(1),
            inventory.items.size(),
            "material stack slot count");

        require_equal(
            static_cast<std::uint32_t>(350),
            inventory.items.front().quantity,
            "material stack quantity");

        require_inventory_invariants(catalog, inventory, "material stack inventory");

        details.append("slots=1 quantity=350");
    }));

    push_result(run_test("stress.inventory_partial_add_transaction", logger, [&](std::string& details) {
        auto catalog = build_material_item_catalog();
        const auto* definition = catalog.find(
            static_cast<mmo::core::id::ItemTemplateId>(1001));
        require(definition != nullptr, "transaction material template not found");

        mmo::core::entity::Inventory inventory{};
        inventory.limits.enforce_slots = true;
        inventory.limits.max_slots = 1;

        auto existing = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            998);
        existing.item_id = static_cast<mmo::core::id::ItemId>(1);
        require(
            mmo::core::inventory::add_item(catalog, inventory, existing).status ==
                mmo::core::inventory::AddStatus::added,
            "transaction initial stack failed");

        auto duplicate = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            10);
        duplicate.item_id = existing.item_id;

        const auto duplicate_result =
            mmo::core::inventory::add_item(catalog, inventory, duplicate);
        require_equal(
            mmo::core::inventory::AddStatus::duplicated_item_id,
            duplicate_result.status,
            "duplicate live item id status");
        require_equal(
            static_cast<std::uint32_t>(0),
            duplicate_result.quantity_added,
            "duplicate live item id quantity added");
        require_equal(
            static_cast<std::uint32_t>(10),
            duplicate_result.quantity_remaining,
            "duplicate live item id quantity remaining");
        require_equal(
            static_cast<std::uint32_t>(998),
            inventory.items.front().quantity,
            "duplicate rejection must not mutate inventory");

        auto partial = duplicate;
        partial.item_id = static_cast<mmo::core::id::ItemId>(2);
        const auto partial_result =
            mmo::core::inventory::add_item(catalog, inventory, partial);
        require_equal(
            mmo::core::inventory::AddStatus::partially_added,
            partial_result.status,
            "slot-limited partial add status");
        require_equal(
            static_cast<std::uint32_t>(1),
            partial_result.quantity_added,
            "slot-limited partial quantity added");
        require_equal(
            static_cast<std::uint32_t>(9),
            partial_result.quantity_remaining,
            "slot-limited partial quantity remaining");
        require_equal(
            static_cast<std::uint32_t>(999),
            inventory.items.front().quantity,
            "partial add must publish committed quantity");
        require_inventory_invariants(catalog, inventory, "partial transaction inventory");

        details.append("duplicate_atomic=true partial_committed=1 remaining=9");
    }));

    push_result(run_test("stress.inventory_slot_limit", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));
        require(definition != nullptr, "equipment template not found");

        mmo::core::entity::Inventory inventory{};
        inventory.limits.enforce_slots = true;
        inventory.limits.max_slots = 3;

        for (std::uint32_t i = 1; i <= 3; ++i)
        {
            auto instance = mmo::core::item::make_instance(
                *definition,
                mmo::core::time::now(),
                1);

            instance.item_id = static_cast<mmo::core::id::ItemId>(i);

            const auto result = mmo::core::inventory::add_item(catalog, inventory, instance);

            require(
                result.status == mmo::core::inventory::AddStatus::added,
                "item should fit before slot limit");
        }

        auto overflow = mmo::core::item::make_instance(
            *definition,
            mmo::core::time::now(),
            1);

        overflow.item_id = static_cast<mmo::core::id::ItemId>(4);

        const auto overflow_result = mmo::core::inventory::add_item(catalog, inventory, overflow);

        require(
            overflow_result.status == mmo::core::inventory::AddStatus::full,
            "inventory did not reject item after slot limit");

        require_equal(
            static_cast<std::size_t>(3),
            inventory.items.size(),
            "slot limited inventory size");

        require_inventory_invariants(catalog, inventory, "slot limited inventory");

        details.append("max_slots=3 rejected_status=");
        details.append(std::string(mmo::core::inventory::add_status_name(overflow_result.status)));
    }));

    push_result(run_test("stress.inventory_remove_item", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();
        auto inventory = build_inventory(catalog, 10, 1, 2000000);

        require_equal(
            static_cast<std::size_t>(10),
            inventory.items.size(),
            "remove test initial inventory size");

        const auto remove_result = mmo::core::inventory::remove_item_by_id(
            inventory,
            static_cast<mmo::core::id::ItemId>(2000001),
            1,
            false);

        require(
            remove_result.status == mmo::core::inventory::RemoveStatus::removed,
            "inventory item was not removed");

        require_equal(
            static_cast<std::size_t>(9),
            inventory.items.size(),
            "remove test final inventory size");

        const auto missing_result = mmo::core::inventory::remove_item_by_id(
            inventory,
            static_cast<mmo::core::id::ItemId>(9999999),
            1,
            false);

        require(
            missing_result.status == mmo::core::inventory::RemoveStatus::not_found,
            "missing item removal should return not_found");

        require_inventory_invariants(catalog, inventory, "remove item inventory");

        details.append("before=10 after=9 missing_status=");
        details.append(std::string(mmo::core::inventory::remove_status_name(missing_result.status)));
    }));

    push_result(run_test("stress.inventory_validation_matrix", logger, [&](std::string& details) {
        auto equipment_catalog = build_item_catalog();
        auto material_catalog = build_material_item_catalog();

        const auto* equipment_definition =
            equipment_catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));

        require(equipment_definition != nullptr, "equipment template not found");

        const auto* material_definition =
            material_catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1001));

        require(material_definition != nullptr, "material template not found");

        const auto make_equipment_instance = [&](mmo::core::id::ItemId item_id, std::uint32_t quantity = 1) {
            mmo::core::item::Instance instance{};
            instance.item_id = item_id;
            instance.item_template_id = equipment_definition->identity.item_template_id;
            instance.quantity = quantity;
            return instance;
        };

        const auto make_material_instance = [&](mmo::core::id::ItemId item_id, std::uint32_t quantity = 1) {
            mmo::core::item::Instance instance{};
            instance.item_id = item_id;
            instance.item_template_id = material_definition->identity.item_template_id;
            instance.quantity = quantity;
            return instance;
        };

        {
            mmo::core::entity::Inventory inventory{};
            inventory.items.push_back(make_equipment_instance(mmo::core::id::invalid_item_id));

            require_equal(
                mmo::core::inventory::ValidationIssue::invalid_item_id,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory invalid_item_id");
        }

        {
            mmo::core::entity::Inventory inventory{};
            auto instance = make_equipment_instance(static_cast<mmo::core::id::ItemId>(2));
            instance.item_template_id = static_cast<mmo::core::id::ItemTemplateId>(9999);
            inventory.items.push_back(instance);

            require_equal(
                mmo::core::inventory::ValidationIssue::invalid_template,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory invalid_template");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(3)));
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(3)));

            require_equal(
                mmo::core::inventory::ValidationIssue::duplicated_item_id,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory duplicated_item_id");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.items.push_back(make_material_instance(static_cast<mmo::core::id::ItemId>(4), 0));

            require_equal(
                mmo::core::inventory::ValidationIssue::invalid_quantity,
                mmo::core::inventory::validate(material_catalog, inventory),
                "inventory invalid_quantity");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.items.push_back(make_material_instance(static_cast<mmo::core::id::ItemId>(5), 1000));

            require_equal(
                mmo::core::inventory::ValidationIssue::stack_exceeds_max,
                mmo::core::inventory::validate(material_catalog, inventory),
                "inventory stack_exceeds_max");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(6), 2));

            require_equal(
                mmo::core::inventory::ValidationIssue::equipment_quantity_not_one,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory equipment_quantity_not_one");
        }

        {
            mmo::core::entity::Inventory inventory{};
            auto instance = make_material_instance(static_cast<mmo::core::id::ItemId>(7));
            instance.equipped = true;
            inventory.items.push_back(instance);

            require_equal(
                mmo::core::inventory::ValidationIssue::equipped_non_equipment,
                mmo::core::inventory::validate(material_catalog, inventory),
                "inventory equipped_non_equipment");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.limits.enforce_slots = true;
            inventory.limits.max_slots = 1;
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(8)));
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(9)));

            require_equal(
                mmo::core::inventory::ValidationIssue::slot_limit_exceeded,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory slot_limit_exceeded");
        }

        {
            mmo::core::entity::Inventory inventory{};
            inventory.limits.enforce_weight = true;
            inventory.limits.max_weight = 1;
            inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(10)));

            require_equal(
                mmo::core::inventory::ValidationIssue::weight_limit_exceeded,
                mmo::core::inventory::validate(equipment_catalog, inventory),
                "inventory weight_limit_exceeded");
        }

        details.append("cases=9");
    }));

    push_result(run_test("stress.inventory_mixed_operations_invariants", logger, [&](std::string& details) {
        auto catalog = build_material_item_catalog();

        const auto* equipment_definition =
            catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));

        const auto* material_definition =
            catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1001));

        require(equipment_definition != nullptr, "equipment template not found");
        require(material_definition != nullptr, "material template not found");

        mmo::core::entity::Inventory inventory{};
        inventory.limits.enforce_slots = true;
        inventory.limits.max_slots = 128;

        std::uint64_t next_item_id = 1;
        std::uint32_t add_attempts = 0;
        std::uint32_t successful_adds = 0;
        std::uint32_t remove_attempts = 0;
        std::uint32_t successful_removes = 0;

        for (std::uint32_t i = 0; i < 5000; ++i)
        {
            ++add_attempts;

            if ((i % 3) == 0)
            {
             auto instance = mmo::core::item::make_instance(
                *material_definition,
                mmo::core::time::now(),
                1 + (i % 50));

            instance.item_id = static_cast<mmo::core::id::ItemId>(next_item_id++);

            const auto result = mmo::core::inventory::add_item(catalog, inventory, instance);

            const bool material_add_status_ok =
                result.status == mmo::core::inventory::AddStatus::added ||
                result.status == mmo::core::inventory::AddStatus::stacked ||
                result.status == mmo::core::inventory::AddStatus::partially_added ||
                result.status == mmo::core::inventory::AddStatus::full;

            if (!material_add_status_ok)
            {
                std::string error;
                error.append("unexpected material add status status=");
                error.append(std::string(mmo::core::inventory::add_status_name(result.status)));
                error.append(" iteration=");
                error.append(std::to_string(i));
                error.append(" quantity=");
                error.append(std::to_string(instance.quantity));
                error.append(" current_slots=");
                error.append(std::to_string(inventory.items.size()));

                throw std::runtime_error(error);
            }

            if (result.status != mmo::core::inventory::AddStatus::full)
            {
                ++successful_adds;
            }

            }
            else
            {
                auto instance = mmo::core::item::make_instance(
                    *equipment_definition,
                    mmo::core::time::now(),
                    1);

                instance.item_id = static_cast<mmo::core::id::ItemId>(next_item_id++);

                const auto result = mmo::core::inventory::add_item(catalog, inventory, instance);

                require(
                    result.status == mmo::core::inventory::AddStatus::added ||
                    result.status == mmo::core::inventory::AddStatus::full,
                    "unexpected equipment add status");

                if (result.status != mmo::core::inventory::AddStatus::full)
                {
                    ++successful_adds;
                }
            }

            if ((i % 7) == 0 && !inventory.items.empty())
            {
                ++remove_attempts;

                const auto item_id = inventory.items.front().item_id;

                const auto result = mmo::core::inventory::remove_item_by_id(
                    inventory,
                    item_id,
                    1,
                    false);

                require(
                    result.status == mmo::core::inventory::RemoveStatus::removed ||
                    result.status == mmo::core::inventory::RemoveStatus::partially_removed,
                    "unexpected remove status");

                require_at_least(
                    1u,
                    result.quantity_removed,
                    "remove operation quantity removed");

                ++successful_removes;

            }

            require_inventory_invariants(catalog, inventory, "mixed inventory operation");
        }

        require_inventory_invariants(catalog, inventory, "final mixed inventory");

        details.append("add_attempts=");
        details.append(std::to_string(add_attempts));
        details.append(" successful_adds=");
        details.append(std::to_string(successful_adds));
        details.append(" remove_attempts=");
        details.append(std::to_string(remove_attempts));
        details.append(" successful_removes=");
        details.append(std::to_string(successful_removes));
        details.append(" final_slots=");
        details.append(std::to_string(inventory.items.size()));
    }));

    // =========================================================================
    // Damage/material contract tests.
    // =========================================================================

    push_result(run_test("stress.damage_kind_classification", logger, [&](std::string& details) {
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::slash), "slash should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::pierce), "pierce should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::sever), "sever should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::impact), "impact should be physical");
        require(!mmo::core::damage::is_physical(mmo::core::damage::Kind::magic), "magic should not be physical");
        require(!mmo::core::damage::is_physical(mmo::core::damage::Kind::elemental), "elemental should not be physical");

        details.append("physical=4 non_physical=2");
    }));

    push_result(run_test("stress.numeric_saturation_boundaries", logger, [&](std::string& details) {
        constexpr auto int32_max = std::numeric_limits<std::int32_t>::max();
        constexpr auto int32_min = std::numeric_limits<std::int32_t>::min();
        constexpr auto uint32_max = std::numeric_limits<std::uint32_t>::max();

        require_equal(
            int32_max,
            mmo::core::numeric::saturating_add(int32_max, 1),
            "signed positive saturation");
        require_equal(
            int32_min,
            mmo::core::numeric::saturating_add(int32_min, -1),
            "signed negative saturation");
        require_equal(
            uint32_max,
            mmo::core::numeric::saturating_add(uint32_max, 1u),
            "unsigned saturation");

        mmo::core::stat::Primary extreme_primary{};
        extreme_primary.vitality = int32_max;
        extreme_primary.strength = int32_max;
        extreme_primary.agility = int32_max;
        extreme_primary.intellect = int32_max;
        extreme_primary.faith = int32_max;
        extreme_primary.dexterity = int32_max;
        extreme_primary.luck = int32_max;
        const auto extreme_derived = mmo::core::stat::derive(extreme_primary);
        require_equal(int32_max, extreme_derived.max_hp, "derived max hp saturation");
        require_equal(int32_max, extreme_derived.attack, "derived attack saturation");

        mmo::core::status::Modifiers lhs{};
        lhs.health_delta_per_tick = int32_max;
        lhs.primary_delta.vitality = int32_min;
        lhs.attack_percent_delta = int32_max;

        mmo::core::status::Modifiers rhs{};
        rhs.health_delta_per_tick = 1;
        rhs.primary_delta.vitality = -1;
        rhs.attack_percent_delta = 1;

        const auto combined = mmo::core::status::add_modifiers(lhs, rhs);
        require_equal(int32_max, combined.health_delta_per_tick, "periodic modifier saturation");
        require_equal(int32_min, combined.primary_delta.vitality, "primary modifier saturation");
        require_equal(int32_max, combined.attack_percent_delta, "percent modifier saturation");

        mmo::core::status::Table statuses{};
        mmo::core::status::Instance stacked{};
        stacked.kind = mmo::core::status::Kind::haste;
        stacked.stacking_mode = mmo::core::status::StackingMode::stack;
        stacked.max_stacks = uint32_max;
        stacked.stacks = uint32_max;
        statuses.apply(stacked);
        stacked.stacks = 1;
        statuses.apply(stacked);
        require_equal(uint32_max, statuses.find(stacked.kind)->stacks, "status stack saturation");

        mmo::core::status::Definition extreme_build_up{};
        extreme_build_up.delivery = mmo::core::status::Delivery::build_up;
        extreme_build_up.build_up_threshold = uint32_max;
        extreme_build_up.build_up_cap_percent = uint32_max;
        const auto extreme_meter = mmo::core::status::make_meter(
            extreme_build_up,
            mmo::core::time::now());
        require_equal(uint32_max, extreme_meter.capacity, "status meter capacity saturation");

        auto material_definition = make_map_material_definition();
        material_definition.resistance.impact_resistance_percent = int32_min;
        mmo::core::damage::Packet material_packet{};
        material_packet.kind = mmo::core::damage::Kind::impact;
        material_packet.amount = int32_max;
        material_packet.penetration_percent = int32_max;
        require_equal(
            int32_max,
            mmo::core::material::resolve_damage(material_definition, material_packet),
            "material damage saturation");

        mmo::core::combat::DamageProfile damage_profile{};
        damage_profile.packet = material_packet;
        damage_profile.critical = true;
        damage_profile.critical_multiplier_percent = int32_max;

        mmo::core::combat::DefenseProfile defense_profile{};
        defense_profile.physical_reduction_percent = int32_min;
        defense_profile.flat_reduction = int32_min;
        defense_profile.shield_current = 1;

        const auto damage_result = mmo::core::combat::resolve_damage(
            damage_profile,
            defense_profile);
        require_equal(int32_max, damage_result.incoming_damage, "incoming damage saturation");
        require_equal(int32_max, damage_result.mitigated_damage, "mitigated damage saturation");
        require_equal(int32_max - 1, damage_result.health_damage, "health damage after shield");

        mmo::world::World world{};
        const auto now = mmo::core::time::now();
        require(
            world.spawn_entity(900001, make_stress_blueprint(), 1, now),
            "numeric boundary entity spawn");
        auto* record = world.entities.find(900001);
        require(record != nullptr, "numeric boundary entity lookup");

        record->resources.shield_current = int32_max;
        const auto health_before = record->resources.health_current;
        require(world.entities.adjust_health(900001, int32_min), "minimum health delta adjustment");
        require_equal(0, record->resources.shield_current, "minimum health delta consumes shield");
        require_equal(health_before - 1, record->resources.health_current, "minimum health delta remainder");
        require(world.entities.adjust_health(900001, int32_max), "maximum health delta adjustment");
        require_equal(record->stats.current_derived.max_hp, record->resources.health_current, "health upper clamp");
        require(world.entities.adjust_mana(900001, int32_min), "minimum mana delta adjustment");
        require_equal(0, record->resources.mana_current, "mana lower clamp");
        require(world.entities.adjust_mana(900001, int32_max), "maximum mana delta adjustment");
        require_equal(record->stats.current_derived.max_mana, record->resources.mana_current, "mana upper clamp");

        details.append("signed_unsigned_resources_damage=covered");
    }));

    push_result(run_test("stress.material_damage_resolution", logger, [&](std::string& details) {
        const auto definition = make_map_material_definition();

        const std::vector<mmo::core::damage::Packet> packets{
            { mmo::core::damage::Kind::slash, 100, 0, false, false },
            { mmo::core::damage::Kind::pierce, 100, 10, false, false },
            { mmo::core::damage::Kind::sever, 100, 0, false, false },
            { mmo::core::damage::Kind::impact, 100, 0, false, false },
            { mmo::core::damage::Kind::magic, 100, 25, false, false },
            { mmo::core::damage::Kind::elemental, 100, 0, false, false },
            { mmo::core::damage::Kind::impact, 0, 0, false, false },
            { mmo::core::damage::Kind::impact, -10, 0, false, false }
        };

        std::int64_t total = 0;

        for (const auto& packet : packets)
        {
            const auto expected = expected_material_damage(definition, packet);
            const auto actual = mmo::core::material::resolve_damage(definition, packet);

            require_equal(expected, actual, "material resolved damage");

            total += actual;
        }

        details.append("cases=");
        details.append(std::to_string(packets.size()));
        details.append(" total_resolved=");
        details.append(std::to_string(total));
    }));

    push_result(run_test("stress.material_damage_deterministic_fuzz", logger, [&](std::string& details) {
        const auto definition = make_map_material_definition();

        std::mt19937 rng{ 0xC0FFEEu };

        std::uniform_int_distribution<int> kind_dist{ 0, 5 };
        std::uniform_int_distribution<int> amount_dist{ -1000, 10000 };
        std::uniform_int_distribution<int> penetration_dist{ -100, 300 };

        std::uint64_t total_cases = 0;
        std::uint64_t zero_or_negative_cases = 0;
        std::uint64_t positive_cases = 0;
        std::uint64_t total_resolved = 0;

        for (std::uint32_t i = 0; i < stress_config.material_damage_iterations; ++i)
        {
            mmo::core::damage::Packet packet{};
            packet.kind = static_cast<mmo::core::damage::Kind>(kind_dist(rng));
            packet.amount = static_cast<std::int32_t>(amount_dist(rng));
            packet.penetration_percent = static_cast<std::int32_t>(penetration_dist(rng));
            packet.can_ricochet = (i % 2) == 0;
            packet.can_embed = (i % 3) == 0;

            const auto expected = expected_material_damage(definition, packet);
            const auto actual = mmo::core::material::resolve_damage(definition, packet);

            require_equal(expected, actual, "fuzz material resolved damage");
            require(actual >= 0, "material damage should never be negative");

            if (packet.amount <= 0)
            {
                ++zero_or_negative_cases;
                require_equal(0, actual, "zero or negative packet should resolve to zero");
            }
            else
            {
                ++positive_cases;
            }

            total_resolved += static_cast<std::uint64_t>(actual);
            ++total_cases;
        }

        require_at_least(1ull, positive_cases, "fuzz positive case count");

        details.append("cases=");
        details.append(std::to_string(total_cases));
        details.append(" positive_cases=");
        details.append(std::to_string(positive_cases));
        details.append(" zero_or_negative_cases=");
        details.append(std::to_string(zero_or_negative_cases));
        details.append(" total_resolved=");
        details.append(std::to_string(total_resolved));
    }));

    push_result(run_test("stress.material_state_defaults", logger, [&](std::string& details) {
        const auto definition = make_map_material_definition();
        const auto state = mmo::core::material::make_state(definition);

        require_equal(definition.kind, state.kind, "material state kind");
        require_equal(definition.max_durability, state.durability, "material state durability");
        require_equal(definition.max_durability, state.max_durability, "material state max durability");
        require(!state.broken, "material state should start unbroken");

        details.append("kind=");
        details.append(std::to_string(static_cast<std::uint32_t>(state.kind)));
        details.append(" durability=");
        details.append(std::to_string(state.durability));
        details.append(" broken=");
        details.append(state.broken ? "true" : "false");
    }));

    // =========================================================================
    // Event stress and runtime integration tests. Pure scheduler tests live in mmo_unit_tests.
    // =========================================================================

    push_result(run_test("stress.event_scheduler_massive", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        std::uint32_t expected_ready = 0;

        for (std::uint32_t i = 0; i < stress_config.event_count; ++i)
        {
            const auto delay = static_cast<std::int64_t>(i % 1000);

            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now + mmo::core::time::Milliseconds{ delay };
            event.zone_id = static_cast<mmo::core::id::ZoneId>(i % 16);
            event.counter = i;

            scheduler.schedule(event);

            if (delay <= 499)
            {
                ++expected_ready;
            }
        }

        require_equal(
            static_cast<std::size_t>(stress_config.event_count),
            scheduler.size(),
            "scheduled event count");

        const auto ready = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 499 });

        require_equal(
            static_cast<std::size_t>(expected_ready),
            ready.size(),
            "massive ready event count");

        require_equal(
            static_cast<std::size_t>(stress_config.event_count - expected_ready),
            scheduler.size(),
            "massive remaining event count");

        const auto rest = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 2000 });

        require_equal(
            static_cast<std::size_t>(stress_config.event_count - expected_ready),
            rest.size(),
            "massive rest event count");

        require(scheduler.empty(), "massive scheduler should be empty");

        details.append("scheduled=");
        details.append(std::to_string(stress_config.event_count));
        details.append(" first_pop=");
        details.append(std::to_string(ready.size()));
        details.append(" second_pop=");
        details.append(std::to_string(rest.size()));
    }));

    push_result(run_test("stress.event_validation_and_scheduler_edges", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        require_equal(static_cast<std::size_t>(0), scheduler.count_ready(now), "empty scheduler ready count");
        require(!scheduler.next_due().has_value(), "empty scheduler next_due");
        require(scheduler.pop_ready(now).empty(), "empty scheduler pop_ready");

        mmo::core::event::Event evolution{};
        evolution.type = mmo::core::event::Type::evolution_due;
        evolution.due_at = now;

        require_equal(
            mmo::core::event::ValidationIssue::missing_entity,
            mmo::core::event::validate(evolution),
            "evolution_due missing entity");

        require(!scheduler.try_schedule(evolution), "invalid evolution event should be rejected");

        mmo::core::event::Event migration{};
        migration.type = mmo::core::event::Type::migration_completed;
        migration.due_at = now;
        migration.entity_id = static_cast<mmo::core::id::EntityId>(1);

        require_equal(
            mmo::core::event::ValidationIssue::missing_zone,
            mmo::core::event::validate(migration),
            "migration_completed missing zone");

        require(!scheduler.try_schedule(migration), "invalid migration event should be rejected");

        mmo::core::event::Event zone_wake{};
        zone_wake.type = mmo::core::event::Type::zone_wake;
        zone_wake.due_at = now;

        require_equal(
            mmo::core::event::ValidationIssue::missing_zone,
            mmo::core::event::validate(zone_wake),
            "zone_wake missing zone");

        require(!scheduler.try_schedule(zone_wake), "invalid zone_wake event should be rejected");

        mmo::core::event::Event region_notice{};
        region_notice.type = mmo::core::event::Type::region_notice;
        region_notice.due_at = now + mmo::core::time::Milliseconds{ 10 };
        region_notice.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        region_notice.counter = 7;

        require(mmo::core::event::is_valid(region_notice), "region_notice should be valid");
        require(scheduler.try_schedule(region_notice), "valid region notice should schedule");

        require_equal(
            static_cast<std::size_t>(0),
            scheduler.count_ready(now - mmo::core::time::Milliseconds{ 1 }),
            "ready count before due time");

        require_equal(
            static_cast<std::size_t>(0),
            scheduler.count_ready(now),
            "ready count before scheduled due time");

        require(scheduler.next_due().has_value(), "next_due should be present after scheduling");

        const auto ready = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 20 });

        require_equal(static_cast<std::size_t>(1), ready.size(), "ready pop count");
        require_equal(static_cast<std::uint32_t>(7), ready.front().counter, "ready event counter");
        require(scheduler.empty(), "scheduler should be empty after pop");

        details.append("rejections=3 ready=1");
    }));

    push_result(run_test("stress.runtime_event_dispatch_exactly_once", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto now = mmo::core::time::now();
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        constexpr auto source_zone = static_cast<mmo::core::id::ZoneId>(10);
        constexpr auto destination_zone = static_cast<mmo::core::id::ZoneId>(20);

        world.spawn_entity(entity_id, make_stress_blueprint(), source_zone, now);

        mmo::core::event::Event migration{};
        migration.type = mmo::core::event::Type::migration_completed;
        migration.due_at = now;
        migration.entity_id = entity_id;
        migration.zone_id = destination_zone;

        mmo::core::event::Event wake{};
        wake.type = mmo::core::event::Type::zone_wake;
        wake.due_at = now;
        wake.zone_id = destination_zone;

        mmo::core::event::Event notice{};
        notice.type = mmo::core::event::Type::region_notice;
        notice.due_at = now;
        notice.zone_id = destination_zone;
        notice.counter = 99;

        mmo::core::event::Event future_notice = notice;
        future_notice.due_at = now + mmo::core::time::Milliseconds{ 1 };
        future_notice.counter = 100;

        require(world.scheduler.try_schedule(migration), "migration should schedule");
        require(world.scheduler.try_schedule(wake), "zone wake should schedule");
        require(world.scheduler.try_schedule(notice), "notice should schedule");
        require(world.scheduler.try_schedule(future_notice), "future notice should schedule");

        const auto first_dispatch = mmo::world::dispatch_ready_events(world, now);

        require_equal(static_cast<std::size_t>(3), first_dispatch.total(), "first dispatch total");
        require_equal(static_cast<std::size_t>(2), first_dispatch.applied, "first dispatch applied");
        require_equal(static_cast<std::size_t>(1), first_dispatch.queued, "first dispatch queued");
        require_equal(static_cast<std::size_t>(0), first_dispatch.rejected, "first dispatch rejected");
        require_equal(static_cast<std::size_t>(1), world.scheduler.size(), "future event retained");
        require_equal(static_cast<std::size_t>(1), world.event_outbox.size(), "domain outbox count");
        require_equal(static_cast<std::uint32_t>(99), world.event_outbox.front().counter, "outbox payload");

        const auto* entity = world.entities.find(entity_id);
        require(entity != nullptr, "migrated entity should exist");
        require_equal(destination_zone, entity->placement.zone_id(), "migration destination");

        const auto* zone = world.zones.get(destination_zone);
        require(zone != nullptr && zone->is_active(), "zone wake should activate destination");

        const auto duplicate_dispatch = mmo::world::dispatch_ready_events(world, now);
        require_equal(static_cast<std::size_t>(0), duplicate_dispatch.total(), "events must dispatch once");
        require_equal(static_cast<std::size_t>(1), world.event_outbox.size(), "outbox must not duplicate");

        details.append("applied=2 queued=1 future=1");
    }));

    push_result(run_test("stress.event_same_due_time_stability", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        constexpr std::uint32_t event_count = 1000;

        for (std::uint32_t i = 0; i < event_count; ++i)
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now;
            event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
            event.counter = i;

            scheduler.schedule(event);
        }

        require_equal(
            static_cast<std::size_t>(event_count),
            scheduler.size(),
            "same due time scheduled count");

        const auto ready = scheduler.pop_ready(now);

        require_equal(
            static_cast<std::size_t>(event_count),
            ready.size(),
            "same due time ready count");

        require(scheduler.empty(), "scheduler should be empty after same due time pop");

        std::unordered_set<std::uint32_t> counters;
        counters.reserve(event_count);

        for (const auto& event : ready)
        {
            counters.insert(event.counter);
        }

        require_equal(
            static_cast<std::size_t>(event_count),
            counters.size(),
            "same due time unique event counters");

        details.append("same_due_events=");
        details.append(std::to_string(event_count));
    }));

    push_result(run_test("stress.event_invalid_mass_rejection", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        constexpr std::uint32_t invalid_count = 1000;
        std::uint32_t rejected = 0;

        for (std::uint32_t i = 0; i < invalid_count; ++i)
        {
            mmo::core::event::Event event{};

            if ((i % 3) == 0)
            {
                event.type = mmo::core::event::Type::evolution_due;
                event.due_at = now;
            }
            else if ((i % 3) == 1)
            {
                event.type = mmo::core::event::Type::migration_completed;
                event.due_at = now;
                event.entity_id = static_cast<mmo::core::id::EntityId>(i + 1);
            }
            else
            {
                event.type = mmo::core::event::Type::zone_wake;
                event.due_at = now;
            }

            if (!scheduler.try_schedule(event))
            {
                ++rejected;
            }
        }

        require_equal(invalid_count, rejected, "invalid event rejection count");
        require(scheduler.empty(), "invalid events should not remain in scheduler");

        details.append("invalid_events=");
        details.append(std::to_string(invalid_count));
        details.append(" rejected=");
        details.append(std::to_string(rejected));
    }));

    // =========================================================================
    // Equipment affix aggregation and experience curve tests.
    // =========================================================================

    push_result(run_test("stress.affix_scaling", logger, [&](std::string& details) {
        mmo::core::item::EquipmentModifierRule rule{};
        rule.effect_key = "stress.affix";
        rule.min_tier = 1;
        rule.max_tier = stress_config.max_tier;
        rule.modifiers.attack_percent_delta = 12;
        rule.modifiers.defense_percent_delta = 8;
        rule.modifiers.move_speed_percent_delta = -6;

        std::vector<mmo::core::item::EquipmentAffixInstance> affixes;
        affixes.reserve(stress_config.affix_count);

        for (std::uint32_t i = 0; i < stress_config.affix_count; ++i)
        {
            const auto tier = (i % stress_config.max_tier) + 1;
            affixes.push_back(mmo::core::item::make_equipment_affix_instance(rule, tier));
        }

        const auto modifiers = mmo::core::item::aggregate_equipment_affixes(affixes);

        require(modifiers.attack_percent_delta > 0, "affix aggregate produced non-positive attack modifier");
        require(modifiers.defense_percent_delta > 0, "affix aggregate produced non-positive defense modifier");
        require(modifiers.move_speed_percent_delta < 0, "affix aggregate produced non-negative move speed modifier");

        auto reversed = affixes;
        std::reverse(reversed.begin(), reversed.end());

        const auto reversed_modifiers = mmo::core::item::aggregate_equipment_affixes(reversed);

        require_equal(
            modifiers.attack_percent_delta,
            reversed_modifiers.attack_percent_delta,
            "affix attack order independence");

        require_equal(
            modifiers.defense_percent_delta,
            reversed_modifiers.defense_percent_delta,
            "affix defense order independence");

        require_equal(
            modifiers.move_speed_percent_delta,
            reversed_modifiers.move_speed_percent_delta,
            "affix move speed order independence");

        details.append("attack_percent_delta=");
        details.append(std::to_string(modifiers.attack_percent_delta));
        details.append(" defense_percent_delta=");
        details.append(std::to_string(modifiers.defense_percent_delta));
        details.append(" move_speed_percent_delta=");
        details.append(std::to_string(modifiers.move_speed_percent_delta));
        details.append(" affix_count=");
        details.append(std::to_string(stress_config.affix_count));
    }));

    push_result(run_test("stress.affix_empty", logger, [&](std::string& details) {
        std::vector<mmo::core::item::EquipmentAffixInstance> affixes;

        const auto modifiers = mmo::core::item::aggregate_equipment_affixes(affixes);

        require_equal(0, modifiers.attack_percent_delta, "empty affix attack modifier");
        require_equal(0, modifiers.defense_percent_delta, "empty affix defense modifier");
        require_equal(0, modifiers.move_speed_percent_delta, "empty affix move speed modifier");

        details.append("empty_affix_modifiers_ok");
    }));

    push_result(run_test("stress.experience_curve", logger, [&](std::string& details) {
        std::uint64_t total_required = 0;
        std::uint64_t previous_required = 0;
        std::uint32_t reset_count = 0;

        for (std::uint32_t level = 1; level <= stress_config.max_level; ++level)
        {
            const auto required = mmo::core::experience::required_for_level(level);

            require(required > 0, "required exp is zero");

            if (level > 1 && required < previous_required)
            {
                if (!is_allowed_experience_tier_reset(level))
                {
                    std::string error;
                    error.append("required exp decreased at non-tier boundary level=");
                    error.append(std::to_string(level));

                    throw std::runtime_error(error);
                }

                ++reset_count;
            }

            previous_required = required;
            total_required += static_cast<std::uint64_t>(required);
        }

        const auto accumulated_result = mmo::core::experience::gain(
            1,
            0,
            0,
            total_required);

        require_at_least(
            stress_config.max_level + 1,
            accumulated_result.level,
            "experience accumulated final level");

        details.append("max_level=");
        details.append(std::to_string(stress_config.max_level));
        details.append(" total_required=");
        details.append(std::to_string(total_required));
        details.append(" final_level=");
        details.append(std::to_string(accumulated_result.level));
        details.append(" tier_resets=");
        details.append(std::to_string(reset_count));
    }));

    push_result(run_test("stress.experience_edge_cases", logger, [&](std::string& details) {
        const auto required_zero = mmo::core::experience::required_for_level(0);
        const auto required_one = mmo::core::experience::required_for_level(1);

        require_equal(required_one, required_zero, "level zero required exp clamp");

        const auto reward_zero = mmo::core::experience::reward_for_level(0);
        const auto reward_one = mmo::core::experience::reward_for_level(1);

        require_equal(reward_one, reward_zero, "level zero reward clamp");

        const auto first_required = mmo::core::experience::required_for_level(1);
        const auto second_required = mmo::core::experience::required_for_level(2);
        const auto third_required = mmo::core::experience::required_for_level(3);

        const auto gain_result = mmo::core::experience::gain(
            0,
            0,
            0,
            first_required + second_required + 7);

        require_equal(
            static_cast<std::uint32_t>(3),
            gain_result.level,
            "multi level gain final level");

        require_equal(
            static_cast<std::uint32_t>(2),
            gain_result.levels_gained,
            "multi level gain levels_gained");

        require_equal(
            static_cast<std::uint64_t>(7),
            gain_result.experience,
            "multi level gain leftover exp");

        require_equal(
            third_required,
            gain_result.experience_to_next,
            "multi level gain next threshold");

        details.append("zero_required=");
        details.append(std::to_string(required_zero));
        details.append(" zero_reward=");
        details.append(std::to_string(reward_zero));
        details.append(" final_level=");
        details.append(std::to_string(gain_result.level));
        details.append(" leftover_exp=");
        details.append(std::to_string(gain_result.experience));
    }));

   // =========================================================================
    // Entity lifecycle tests.
    // =========================================================================

    push_result(run_test("stress.entity_spawn_massive", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        for (std::uint32_t i = 1; i <= stress_config.entity_spawn_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "spawned entity was not found");

            require(record->resources.health_current > 0, "spawned entity should have positive hp");
            require(record->stats.current_derived.max_hp > 0, "spawned entity should have positive max hp");
        }

        details.append("entities=");
        details.append(std::to_string(stress_config.entity_spawn_count));
    }));

    push_result(run_test("stress.entity_zone_index_controlled_mutation", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();
        constexpr auto first_entity = static_cast<mmo::core::id::EntityId>(1);
        constexpr auto second_entity = static_cast<mmo::core::id::EntityId>(2);
        constexpr auto first_zone = static_cast<mmo::core::id::ZoneId>(10);
        constexpr auto second_zone = static_cast<mmo::core::id::ZoneId>(20);

        require(
            !world.spawn_entity(mmo::core::id::invalid_entity_id, blueprint, first_zone, now),
            "invalid entity id must be rejected");
        require(
            !world.spawn_entity(first_entity, blueprint, mmo::core::id::invalid_zone_id, now),
            "invalid spawn zone must be rejected");
        require(world.spawn_entity(first_entity, blueprint, first_zone, now), "first spawn failed");
        require(world.spawn_entity(second_entity, blueprint, second_zone, now), "second spawn failed");
        require(world.has_consistent_spatial_state(), "spawned spatial state inconsistent");

        require(world.move_entity_to_zone(first_entity, second_zone, now), "zone move failed");
        require(world.entities.ids_in_zone(first_zone).empty(), "old zone retained moved entity");
        require_equal(
            static_cast<std::size_t>(2),
            world.entities.ids_in_zone(second_zone).size(),
            "destination zone entity count");
        require(world.has_consistent_spatial_state(), "spatial state inconsistent after move");

        require(world.move_entity_to_zone(first_entity, second_zone, now), "idempotent zone move failed");
        require_equal(
            static_cast<std::size_t>(2),
            world.entities.ids_in_zone(second_zone).size(),
            "idempotent move duplicated index entry");
        require(
            !world.move_entity_to_zone(first_entity, mmo::core::id::invalid_zone_id, now),
            "invalid destination zone must be rejected");
        require(
            !world.move_entity_to_zone(
                static_cast<mmo::core::id::EntityId>(999),
                first_zone,
                now),
            "missing entity move must fail");
        require(world.has_consistent_spatial_state(), "rejected moves changed spatial state");

        require(world.erase_entity(second_entity, now), "indexed entity erase failed");
        require(world.has_consistent_spatial_state(), "spatial state inconsistent after erase");

        details.append("spawn_rejections=2 moves=1 idempotent=1 erase=1");
    }));


    push_result(run_test("stress.entity_contract_edges", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();
        const auto entity_id = static_cast<mmo::core::id::EntityId>(1);

        world.spawn_entity(entity_id, blueprint, 1, now);

        auto* record = world.entities.find(entity_id);
        require(record != nullptr, "entity contract target not found");

        const auto* equipment_definition =
            catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));

        require(equipment_definition != nullptr, "entity contract equipment template not found");

        const auto make_equipment_instance = [&](mmo::core::id::ItemId item_id) {
            mmo::core::item::Instance instance{};
            instance.item_id = item_id;
            instance.item_template_id = equipment_definition->identity.item_template_id;
            instance.quantity = 1;
            return instance;
        };

        record->inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(1)));
        record->inventory.items.push_back(make_equipment_instance(static_cast<mmo::core::id::ItemId>(1)));

        require_equal(
            mmo::core::inventory::ValidationIssue::duplicated_item_id,
            world.entities.validate_inventory(entity_id, catalog),
            "entity inventory wrapper duplicate");

        require_equal(
            mmo::core::inventory::ValidationIssue::invalid_item_id,
            world.entities.validate_inventory(static_cast<mmo::core::id::EntityId>(9999), catalog),
            "missing entity inventory validation");

        const auto hp_before = record->resources.health_current;

        mmo::core::combat::DamageProfile zero_profile{};
        zero_profile.packet.kind = mmo::core::damage::Kind::impact;
        zero_profile.packet.amount = 0;

        require(
            world.entities.apply_damage(entity_id, zero_profile, now).has_value(),
            "zero damage should still resolve against a valid entity");

        require_equal(hp_before, record->resources.health_current, "zero damage should not change hp");

        mmo::core::combat::DamageProfile negative_profile = zero_profile;
        negative_profile.packet.amount = -10;

        require(
            world.entities.apply_damage(entity_id, negative_profile, now).has_value(),
            "negative damage should still resolve against a valid entity");

        require_equal(hp_before, record->resources.health_current, "negative damage should not change hp");
        require(record->lifecycle.last_damage_at.has_value(), "damage should record last_damage_at");

        require(
            world.entities.apply_damage(
                static_cast<mmo::core::id::EntityId>(9999),
                zero_profile,
                now) == std::nullopt,
            "missing target damage should return nullopt");

        details.append("duplicate_inventory=true zero_and_negative_damage=true missing_target=true");
    }));

    // =========================================================================
    // Combat contract tests.
    // =========================================================================

     push_result(run_test("stress.combat_damage_multiple_entities", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        std::uint64_t total_damage = 0;
        std::uint32_t killed = 0;
        std::uint32_t lethal_hits = 0;

        for (std::uint32_t i = 1; i <= stress_config.combat_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto* before = world.entities.find(entity_id);
            require(before != nullptr, "combat entity not found before damage");

            const auto hp_before = before->resources.health_current;

            mmo::core::damage::Packet packet{};
            packet.kind = static_cast<mmo::core::damage::Kind>(i % 6);

            const bool lethal = (i == 1) || ((i % 5) == 0);

            packet.amount = lethal
                ? static_cast<std::int32_t>(hp_before + 5)
                : static_cast<std::int32_t>(1 + (i % 3));

            if (lethal)
            {
                ++lethal_hits;
            }

            require(
                apply_combat_damage_to_entity(world, entity_id, packet, entity_id),
                "damage application failed");

            auto* after = world.entities.find(entity_id);
            require(after != nullptr, "combat entity not found after damage");

            const auto expected_hp =
                std::clamp(
                    hp_before - packet.amount,
                    0,
                    before->stats.current_derived.max_hp);

            require_equal(expected_hp, after->resources.health_current, "multi entity damage hp");

            require(after->resources.health_current >= 0, "hp should never be below zero");
            require(
                after->resources.health_current <= after->stats.current_derived.max_hp,
                "hp should never exceed max hp");

            total_damage += static_cast<std::uint64_t>(packet.amount);

            if (!after->lifecycle.alive)
            {
                ++killed;
            }

            if (lethal)
            {
                require(!after->lifecycle.alive, "lethal damage did not kill target");
                require(after->lifecycle.killing_trace.has_value(), "missing killing trace");
                require(
                    after->lifecycle.killing_source.kind == mmo::core::damage::SourceKind::entity,
                    "killing source kind mismatch");
                require(
                    after->lifecycle.killing_source.entity_id.has_value(),
                    "missing killing source entity id");
                require_equal(
                    entity_id,
                    after->lifecycle.killing_source.entity_id.value(),
                    "killing source entity id");
            }
        }

        require_at_least(1u, killed, "killed count");

        details.append("entities=");
        details.append(std::to_string(stress_config.combat_entity_count));
        details.append(" total_damage=");
        details.append(std::to_string(total_damage));
        details.append(" killed=");
        details.append(std::to_string(killed));
        details.append(" lethal_hits=");
        details.append(std::to_string(lethal_hits));
    }));

    push_result(run_test("stress.combat_damage_many_sources_single_entity", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        const auto target_id = static_cast<mmo::core::id::EntityId>(1);

        world.spawn_entity(target_id, blueprint, 1, now);

        auto* target = world.entities.find(target_id);
        require(target != nullptr, "target entity not found");

        const auto hp_start = target->resources.health_current;

        std::int64_t expected_hp = hp_start;
        std::uint64_t hit_count = 0;
        std::unordered_map<std::uint64_t, std::uint64_t> damage_by_source;

        for (std::uint32_t source = 1; source <= stress_config.combat_sources_per_entity; ++source)
        {
            for (std::uint32_t hit = 1; hit <= stress_config.combat_hits_per_source; ++hit)
            {
                const auto source_id = static_cast<std::uint64_t>(source);
                const auto amount = static_cast<std::int32_t>(1 + ((source + hit) % 4));

                mmo::core::damage::Packet packet{};
                packet.kind = static_cast<mmo::core::damage::Kind>((source + hit) % 6);
                packet.amount = amount;

                require(
                    apply_combat_damage_to_entity(
                        world,
                        target_id,
                        packet,
                        static_cast<mmo::core::id::EntityId>(source)),
                    "single target source damage failed");

                expected_hp = std::max<std::int64_t>(0, expected_hp - amount);
                damage_by_source[source_id] += static_cast<std::uint64_t>(amount);
                ++hit_count;
            }
        }

        target = world.entities.find(target_id);
        require(target != nullptr, "target entity not found after source damage");

        require_equal(
            static_cast<std::int32_t>(expected_hp),
            target->resources.health_current,
            "single entity multi source hp");

        require_equal(
            static_cast<std::size_t>(stress_config.combat_sources_per_entity),
            damage_by_source.size(),
            "single entity source tracking count");

        require(target->resources.health_current >= 0, "target hp below zero");
        require(
            target->resources.health_current <= target->stats.current_derived.max_hp,
            "target hp above max");

        details.append("sources=");
        details.append(std::to_string(stress_config.combat_sources_per_entity));
        details.append(" hits=");
        details.append(std::to_string(hit_count));
        details.append(" hp_start=");
        details.append(std::to_string(hp_start));
        details.append(" hp_end=");
        details.append(std::to_string(target->resources.health_current));
    }));

    push_result(run_test("stress.combat_damage_many_sources_many_entities", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        std::unordered_map<std::uint64_t, std::int64_t> expected_hp_by_entity;

        std::uint64_t total_hits = 0;
        std::uint64_t total_damage = 0;

        const auto forced_kill_target_id = static_cast<mmo::core::id::EntityId>(1);

        const auto forced_source_id = static_cast<mmo::core::id::EntityId>(
            stress_config.combat_entity_count > 1 ? 2 : 1);

        bool forced_kill_applied = false;

        for (std::uint32_t target = 1; target <= stress_config.combat_entity_count; ++target)
        {
            const auto target_id = static_cast<mmo::core::id::EntityId>(target);

            world.spawn_entity(target_id, blueprint, 1, now);

            auto* record = world.entities.find(target_id);
            require(record != nullptr, "many target entity not found");

            expected_hp_by_entity[static_cast<std::uint64_t>(target_id)] =
                record->resources.health_current;
        }

        for (std::uint32_t target = 1; target <= stress_config.combat_entity_count; ++target)
        {
            const auto target_id = static_cast<mmo::core::id::EntityId>(target);

            for (std::uint32_t source = 1; source <= stress_config.combat_sources_per_entity; ++source)
            {
                for (std::uint32_t hit = 1; hit <= stress_config.combat_hits_per_source; ++hit)
                {
                    const auto amount = static_cast<std::int32_t>(
                        1 + ((target + source + hit) % 3));

                    mmo::core::damage::Packet packet{};
                    packet.kind = static_cast<mmo::core::damage::Kind>(
                        (target + source + hit) % 6);
                    packet.amount = amount;

                    require(
                        apply_combat_damage_to_entity(
                            world,
                            target_id,
                            packet,
                            static_cast<mmo::core::id::EntityId>(source)),
                        "many target damage failed");

                    auto& expected_hp =
                        expected_hp_by_entity[static_cast<std::uint64_t>(target_id)];

                    expected_hp = std::max<std::int64_t>(0, expected_hp - amount);

                    ++total_hits;
                    total_damage += static_cast<std::uint64_t>(amount);
                }
            }

            if (target_id == forced_kill_target_id)
            {
                auto& expected_hp =
                    expected_hp_by_entity[static_cast<std::uint64_t>(target_id)];

                if (expected_hp > 0)
                {
                    mmo::core::damage::Packet lethal{};
                    lethal.kind = mmo::core::damage::Kind::impact;
                    lethal.amount = static_cast<std::int32_t>(expected_hp + 5);

                    require(
                        apply_combat_damage_to_entity(
                            world,
                            target_id,
                            lethal,
                            forced_source_id),
                        "forced lethal damage failed");

                    expected_hp = 0;
                    ++total_hits;
                    total_damage += static_cast<std::uint64_t>(lethal.amount);
                    forced_kill_applied = true;
                }
            }
        }

        std::uint32_t killed = 0;

        for (std::uint32_t target = 1; target <= stress_config.combat_entity_count; ++target)
        {
            const auto target_id = static_cast<mmo::core::id::EntityId>(target);
            auto* record = world.entities.find(target_id);

            require(record != nullptr, "many target entity missing after damage");

            const auto expected_hp =
                static_cast<std::int32_t>(
                    expected_hp_by_entity[static_cast<std::uint64_t>(target_id)]);

            require_equal(expected_hp, record->resources.health_current, "many source many entity hp");

            require(record->resources.health_current >= 0, "many entity hp below zero");
            require(
                record->resources.health_current <= record->stats.current_derived.max_hp,
                "many entity hp above max");

            if (forced_kill_applied && target_id == forced_kill_target_id)
            {
                require(!record->lifecycle.alive, "forced killed target should be dead");
                require(record->lifecycle.death_at.has_value(), "death_at should be recorded");
                require(record->lifecycle.killer_entity_id.has_value(), "killer should be recorded");
                require(record->lifecycle.killing_trace.has_value(), "killing trace should be recorded");

                require_equal(
                    forced_source_id,
                    record->lifecycle.killer_entity_id.value(),
                    "forced kill killer id");
            }

            if (!record->lifecycle.alive)
            {
                ++killed;
            }
        }

        require_at_least(1u, killed, "killed count");

        details.append("targets=");
        details.append(std::to_string(stress_config.combat_entity_count));
        details.append(" sources_per_target=");
        details.append(std::to_string(stress_config.combat_sources_per_entity));
        details.append(" total_hits=");
        details.append(std::to_string(total_hits));
        details.append(" total_damage=");
        details.append(std::to_string(total_damage));
        details.append(" killed=");
        details.append(std::to_string(killed));
        details.append(" forced_kill=");
        details.append(forced_kill_applied ? "true" : "false");
    }));

    push_result(run_test("stress.combat_dead_entity_killer_not_overwritten", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        const auto target_id = static_cast<mmo::core::id::EntityId>(1);
        const auto killer_a = static_cast<mmo::core::id::EntityId>(10);
        const auto killer_b = static_cast<mmo::core::id::EntityId>(20);

        world.spawn_entity(target_id, blueprint, 1, now);

        auto* target = world.entities.find(target_id);
        require(target != nullptr, "target not found before lethal damage");

        mmo::core::damage::Packet first_lethal{};
        first_lethal.kind = mmo::core::damage::Kind::impact;
        first_lethal.amount = target->resources.health_current + 100;

        require(
            apply_combat_damage_to_entity(world, target_id, first_lethal, killer_a),
            "first lethal damage failed");

        target = world.entities.find(target_id);
        require(target != nullptr, "target not found after lethal damage");

        require(!target->lifecycle.alive, "target should be dead after lethal damage");
        require(target->lifecycle.killer_entity_id.has_value(), "killer should be recorded after death");
        require(target->lifecycle.death_at.has_value(), "death_at should be recorded after death");
        require(target->lifecycle.killing_trace.has_value(), "killing trace should be recorded after death");

        const auto original_killer = target->lifecycle.killer_entity_id.value();
        const auto original_death_at = target->lifecycle.death_at;
        const auto original_hp = target->resources.health_current;

        require_equal(killer_a, original_killer, "original killer mismatch");

        mmo::core::damage::Packet overkill{};
        overkill.kind = mmo::core::damage::Kind::slash;
        overkill.amount = 999999;

        const auto overkill_result =
            apply_combat_damage_to_entity(world, target_id, overkill, killer_b);

        static_cast<void>(overkill_result);

        target = world.entities.find(target_id);
        require(target != nullptr, "target not found after overkill attempt");

        require(!target->lifecycle.alive, "dead target became alive unexpectedly");
        require_equal(original_hp, target->resources.health_current, "dead target hp changed after overkill");
        require(target->lifecycle.killer_entity_id.has_value(), "killer lost after overkill");
        require_equal(original_killer, target->lifecycle.killer_entity_id.value(), "killer was overwritten after death");
        require(target->lifecycle.death_at == original_death_at, "death_at was overwritten after death");

        details.append("killer_preserved=true original_killer=");
        details.append(std::to_string(static_cast<std::uint64_t>(original_killer)));
    }));

    push_result(run_test("stress.combat_validation_matrix", logger, [&](std::string& details) {
        const auto now = mmo::core::time::now();
        const auto profile = mmo::core::combat::make_auto_attack_profile();

        const std::optional<mmo::core::combat::TargetView> no_target{};

        mmo::core::combat::ActorView actor{};
        actor.entity_id = static_cast<mmo::core::id::EntityId>(1);
        actor.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        actor.alive = true;
        actor.health_current = 100;
        actor.mana_current = 100;

        mmo::core::combat::Intent intent{};
        intent.kind = profile.kind;
        intent.issued_at = now;

        auto validation = mmo::core::combat::validate(actor, intent, profile, no_target);
        require_equal(mmo::core::combat::ValidationCode::missing_target, validation.code, "combat missing target");

        auto dead_actor = actor;
        dead_actor.alive = false;

        validation = mmo::core::combat::validate(dead_actor, intent, profile, no_target);
        require_equal(mmo::core::combat::ValidationCode::actor_dead, validation.code, "combat actor dead");

        auto health_cost_profile = profile;
        health_cost_profile.cost.health = 5;

        auto low_health_actor = actor;
        low_health_actor.health_current = 4;

        validation = mmo::core::combat::validate(low_health_actor, intent, health_cost_profile, no_target);
        require_equal(mmo::core::combat::ValidationCode::insufficient_health, validation.code, "combat insufficient health");

        auto mana_cost_profile = profile;
        mana_cost_profile.cost.mana = 5;

        auto low_mana_actor = actor;
        low_mana_actor.mana_current = 4;

        validation = mmo::core::combat::validate(low_mana_actor, intent, mana_cost_profile, no_target);
        require_equal(mmo::core::combat::ValidationCode::insufficient_mana, validation.code, "combat insufficient mana");

        mmo::core::combat::TargetView self_target{};
        self_target.entity_id = actor.entity_id;
        self_target.zone_id = actor.zone_id;
        self_target.alive = true;

        intent.target_entity_id = actor.entity_id;

        validation = mmo::core::combat::validate(actor, intent, profile, self_target);
        require_equal(mmo::core::combat::ValidationCode::target_is_self, validation.code, "combat target is self");

        mmo::core::combat::TargetView invalid_target{};
        invalid_target.entity_id = static_cast<mmo::core::id::EntityId>(2);
        invalid_target.zone_id = actor.zone_id;
        invalid_target.alive = true;

        intent.target_entity_id = static_cast<mmo::core::id::EntityId>(3);

        validation = mmo::core::combat::validate(actor, intent, profile, invalid_target);
        require_equal(mmo::core::combat::ValidationCode::invalid_target, validation.code, "combat invalid target");

        mmo::core::combat::TargetView wrong_zone_target{};
        wrong_zone_target.entity_id = static_cast<mmo::core::id::EntityId>(3);
        wrong_zone_target.zone_id = static_cast<mmo::core::id::ZoneId>(2);
        wrong_zone_target.alive = true;

        intent.target_entity_id = wrong_zone_target.entity_id;

        validation = mmo::core::combat::validate(actor, intent, profile, wrong_zone_target);
        require_equal(mmo::core::combat::ValidationCode::target_wrong_zone, validation.code, "combat wrong zone");

        mmo::core::combat::TargetView dead_target = wrong_zone_target;
        dead_target.zone_id = actor.zone_id;
        dead_target.alive = false;

        validation = mmo::core::combat::validate(actor, intent, profile, dead_target);
        require_equal(mmo::core::combat::ValidationCode::target_dead, validation.code, "combat target dead");

        mmo::core::combat::TargetView valid_target = wrong_zone_target;
        valid_target.zone_id = actor.zone_id;
        valid_target.alive = true;

        intent.target_entity_id = valid_target.entity_id;

        validation = mmo::core::combat::validate(actor, intent, profile, valid_target);
        require_equal(mmo::core::combat::ValidationCode::ok, validation.code, "combat valid target");

        const auto resolution = mmo::core::combat::resolve(actor, intent, profile, valid_target);

        require(resolution.validation.ok(), "combat resolve should be valid");
        require_equal(valid_target.entity_id, resolution.final_target_entity_id.value(), "combat resolved target id");
        require_equal(static_cast<std::uint32_t>(100), resolution.planned_threat, "combat planned threat");

        details.append("cases=8 valid=1");
    }));

    // =========================================================================
    // World/status contract tests.
    // =========================================================================

    push_result(run_test("stress.world_status_negative_immunity", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();
        const auto entity_id = static_cast<mmo::core::id::EntityId>(1);

        world.spawn_entity(entity_id, blueprint, 1, now);

        auto* record = world.entities.find(entity_id);
        require(record != nullptr, "status immunity target not found");

        const auto catalog = mmo::core::status::make_status_catalog();

        const auto bless = catalog.instantiate(
            mmo::core::status::Kind::bless,
            now,
            5,
            entity_id);

        require(bless.has_value(), "bless status should instantiate");
        require(world.entities.apply_status(entity_id, bless.value()), "bless status should apply");

        record = world.entities.find(entity_id);
        require(record != nullptr, "status immunity target missing after bless");

        require(
            record->combat.status_effects.find(mmo::core::status::Kind::bless) != nullptr,
            "bless status should remain active");

        const auto status_totals = record->combat.status_effects.aggregate();

        require(
            status_totals.modifiers.negative_status_immunity,
            "bless should grant negative status immunity");

        const auto poison = catalog.instantiate(
            mmo::core::status::Kind::poison,
            now,
            1,
            entity_id);

        require(poison.has_value(), "poison status should instantiate");

        require(
            !world.entities.apply_status(entity_id, poison.value()),
            "poison should be blocked by immunity");

        record = world.entities.find(entity_id);
        require(record != nullptr, "status immunity target missing after poison attempt");

        require(
            record->combat.status_effects.find(mmo::core::status::Kind::poison) == nullptr,
            "poison should not be active");

        details.append("bless_stacks=5 poison_blocked=true");
    }));

    push_result(run_test("stress.status_periodic_cadence_and_catch_up", logger, [&](std::string& details) {
        mmo::world::World world{};
        const auto now = mmo::core::time::now();
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);

        world.spawn_entity(entity_id, make_stress_blueprint(), 1, now);
        auto* record = world.entities.find(entity_id);
        require(record != nullptr, "periodic status target not found");
        const auto initial_health = record->resources.health_current;

        const auto poison_definition = mmo::core::status::make_poison_definition();
        const auto poison = mmo::core::status::make_instance(
            poison_definition,
            now,
            1,
            entity_id);
        require(world.entities.apply_status(entity_id, poison), "periodic poison should apply");

        require_equal(
            static_cast<std::uint64_t>(0),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 999 }).applications,
            "periodic effect before first deadline");
        require_equal(
            initial_health,
            world.entities.find(entity_id)->resources.health_current,
            "health before first periodic deadline");

        require_equal(
            static_cast<std::uint64_t>(1),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 1000 }).applications,
            "periodic first deadline");
        require_equal(
            initial_health - 8,
            world.entities.find(entity_id)->resources.health_current,
            "health after first periodic deadline");

        require_equal(
            static_cast<std::uint64_t>(0),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 1000 }).applications,
            "periodic deadline must be consumed once");

        require_equal(
            static_cast<std::uint64_t>(2),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 3500 }).applications,
            "periodic catch-up count");
        require_equal(
            initial_health - 24,
            world.entities.find(entity_id)->resources.health_current,
            "health after periodic catch-up");

        require_equal(
            static_cast<std::uint64_t>(5),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 8000 }).applications,
            "periodic applications through expiration boundary");
        require_equal(
            initial_health - 64,
            world.entities.find(entity_id)->resources.health_current,
            "health after all periodic applications");
        require(
            world.entities.sweep_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 8000 }),
            "expired periodic status should sweep");
        require_equal(
            static_cast<std::uint64_t>(0),
            world.entities.process_periodic_statuses(
                entity_id,
                now + mmo::core::time::Milliseconds{ 9000 }).applications,
            "expired status must not tick again");

        details.append("ticks=8 damage=64 catch_up=2");
    }));

    push_result(run_test("stress.fixed_timestep_contract", logger, [&](std::string& details) {
        require_equal(
            static_cast<mmo::core::time::Nanoseconds::rep>(16'666'666),
            mmo::core::time::tick_offset(1, 60).count(),
            "60hz first tick offset");
        require_equal(
            static_cast<mmo::core::time::Nanoseconds::rep>(1'000'000'000),
            mmo::core::time::tick_offset(60, 60).count(),
            "60hz one second without drift");
        require_equal(
            static_cast<mmo::core::time::TickCount>(60),
            mmo::core::time::duration_to_ticks(mmo::core::time::Seconds{ 1 }, 60),
            "one second tick conversion");
        require_equal(
            static_cast<std::uint64_t>(7),
            mmo::server::calculate_skipped_ticks(10, 20, 4),
            "bounded lag skip count");
        require_equal(
            static_cast<std::uint64_t>(0),
            mmo::server::calculate_skipped_ticks(20, 10, 4),
            "future simulation does not skip");

        mmo::world::World world{};
        auto catalog = build_item_catalog();
        auto& scheduler = world_event_scheduler(world);
        require(
            world.spawn_entity(800001, make_stress_blueprint(), 1, mmo::core::time::now()),
            "fixed timestep entity spawn");

        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::region_notice;
        event.due_at = mmo::core::time::now() + mmo::core::time::Milliseconds{ 50 };
        event.zone_id = 1;
        require(scheduler.try_schedule(event), "fixed timestep event schedule");

        mmo::server::LoopConfig config{};
        config.tick_rate = 20;
        config.max_ticks = 2;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);
        require_equal(static_cast<std::uint64_t>(2), stats.ticks, "fixed timestep executed ticks");
        require_equal(static_cast<std::uint64_t>(1), stats.events_processed, "scheduled simulation event");
        require_equal(static_cast<std::uint64_t>(1), stats.load_recalculations, "initial dirty load sync");
        require_equal(static_cast<std::uint64_t>(0), stats.late_ticks, "unpaced loop late ticks");
        require_equal(static_cast<std::uint64_t>(0), stats.ticks_skipped, "unpaced loop skipped ticks");

        config.max_ticks = 3;
        const auto clean_stats = mmo::server::run_loop(world, catalog, config, logger);
        require_equal(
            static_cast<std::uint64_t>(0),
            clean_stats.load_recalculations,
            "clean inventory load must not recalculate per tick");

        require(
            world.entities.mark_inventory_load_dirty(800001),
            "explicit inventory dirty mark");
        config.max_ticks = 2;
        const auto dirty_stats = mmo::server::run_loop(world, catalog, config, logger);
        require_equal(
            static_cast<std::uint64_t>(1),
            dirty_stats.load_recalculations,
            "dirty inventory load recalculates exactly once");

        details.append("rate=60 exact_second=true catch_up_limit=4 load_syncs=1/0/1");
    }));

    push_result_with_budget(run_test("stress.world_loop_mixed_entity_states", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        const std::uint32_t entity_count =
            std::min<std::uint32_t>(stress_config.world_entity_count, 2000);

        std::uint32_t expected_dead = 0;
        std::uint32_t with_inventory = 0;
        std::uint32_t with_status = 0;

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "mixed world entity not found after spawn");

            if ((i % 2) == 0)
            {
                record->inventory = build_inventory(
                    catalog,
                    8,
                    1,
                    static_cast<std::uint64_t>(i) * 10000ull);

                mmo::core::entity::sync_load(*record, catalog);
                ++with_inventory;
            }

            if ((i % 3) == 0)
            {
                auto poison_def = mmo::core::status::make_poison_definition();

                auto poison = mmo::core::status::make_instance(
                    poison_def,
                    now,
                    1,
                    entity_id);

                require(
                    world.entities.apply_status(entity_id, poison),
                    "poison status should apply in mixed world");

                ++with_status;
            }

            if ((i % 5) == 0)
            {
                mmo::core::damage::Packet lethal{};
                lethal.kind = mmo::core::damage::Kind::impact;
                lethal.amount = record->resources.health_current + 1;

                require(
                    apply_combat_damage_to_entity(world, entity_id, lethal, entity_id),
                    "mixed world lethal damage failed");

                ++expected_dead;
            }
        }

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = std::min<std::uint32_t>(stress_config.world_max_ticks, 120);
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        std::uint32_t actual_dead = 0;

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);
            auto* record = world.entities.find(entity_id);

            require(record != nullptr, "mixed world entity missing after loop");

            if ((i % 5) == 0)
            {
                require(
                    !record->lifecycle.alive,
                    "dead entity became alive unexpectedly in mixed world");
            }

            if (!record->lifecycle.alive)
            {
                ++actual_dead;
            }

            require_inventory_invariants(
                catalog,
                record->inventory,
                "mixed world inventory invariant");

            require(record->resources.health_current >= 0, "entity hp below zero after mixed world loop");

            require(
                record->resources.health_current <= record->stats.current_derived.max_hp,
                "entity hp above max after mixed world loop");
        }

        require_at_least(expected_dead, actual_dead, "mixed world dead entity count");

        details.append("entities=");
        details.append(std::to_string(entity_count));
        details.append(" with_inventory=");
        details.append(std::to_string(with_inventory));
        details.append(" with_status=");
        details.append(std::to_string(with_status));
        details.append(" expected_dead=");
        details.append(std::to_string(expected_dead));
        details.append(" actual_dead=");
        details.append(std::to_string(actual_dead));
        details.append(" ticks=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.ticks)));
        details.append(" entities_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.entities_processed)));
    }), read_env_u64("MMO_BUDGET_WORLD_MIXED_MS", 5000));

    push_result_with_budget(run_test("stress.world_loop_core", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        for (std::uint32_t i = 1; i <= stress_config.world_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "spawned world entity was not found");

            record->inventory = build_inventory(
                catalog,
                stress_config.world_items_per_entity,
                10,
                static_cast<std::uint64_t>(i) * 100000ull);

            mmo::core::entity::sync_load(*record, catalog);

            auto poison_def = mmo::core::status::make_poison_definition();

            auto poison = mmo::core::status::make_instance(
                poison_def,
                now,
                1,
                entity_id);

            const auto status_applied = world.entities.apply_status(entity_id, poison);

            require(
                status_applied,
                "world core poison status should apply");
        }

        auto& scheduler = world_event_scheduler(world);

        const std::uint32_t core_event_count =
            std::min<std::uint32_t>(
                stress_config.event_count,
                stress_config.world_entity_count);

        for (std::uint32_t i = 0; i < core_event_count; ++i)
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now;
            event.zone_id = static_cast<mmo::core::id::ZoneId>((i % 16) + 1);
            event.counter = i;

            require(
                scheduler.try_schedule(event),
                "world core due event should schedule");
        }

        require_equal(
            static_cast<std::size_t>(core_event_count),
            scheduler.size(),
            "world core initial event count");

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = stress_config.world_max_ticks;
        config.sleep = false;

        const auto loop_started = std::chrono::steady_clock::now();

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        const auto loop_finished = std::chrono::steady_clock::now();

        const auto loop_duration_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                loop_finished - loop_started).count());

        const auto entities_per_second =
            loop_duration_ms == 0
                ? std::uint64_t{ 0 }
                : (static_cast<std::uint64_t>(stats.entities_processed) * 1000ull) /
                    loop_duration_ms;

        require_equal(
            static_cast<std::uint64_t>(stress_config.world_max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "world loop ticks");

        const auto expected_entities_processed =
            static_cast<std::uint64_t>(stress_config.world_entity_count) *
            static_cast<std::uint64_t>(stress_config.world_max_ticks);

        require_at_least(
            expected_entities_processed,
            static_cast<std::uint64_t>(stats.entities_processed),
            "world loop entities processed");

        require_at_least(
            static_cast<std::uint64_t>(core_event_count),
            static_cast<std::uint64_t>(stats.events_processed),
            "world loop core should process scheduled due events");

        require(
            scheduler.empty(),
            "world loop core scheduler should be empty after processing due events");

        require(
            stats.status_sweeps > 0,
            "world loop core should process status sweeps");

        const auto minimum_entities_per_second = read_env_u64(
            "MMO_MIN_WORLD_ENTITIES_PER_SECOND",
            stress_config.level == StressLevel::high ? 80000ull : 30000ull);

        require_at_least(
            minimum_entities_per_second,
            entities_per_second,
            "world loop entities per second regression");

        for (std::uint32_t i = 1; i <= stress_config.world_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);
            auto* record = world.entities.find(entity_id);

            require(record != nullptr, "entity missing after world loop");

            require_equal(
                static_cast<std::size_t>(stress_config.world_items_per_entity),
                record->inventory.items.size(),
                "inventory item count changed after world loop");

            require_inventory_invariants(
                catalog,
                record->inventory,
                "inventory validation after world loop");

            require(
                record->resources.health_current >= 0,
                "world entity hp below zero");

            require(
                record->resources.health_current <= record->stats.current_derived.max_hp,
                "world entity hp above max");
        }

        const auto avg_entities_per_tick =
            stats.ticks == 0
                ? std::uint64_t{ 0 }
                : static_cast<std::uint64_t>(stats.entities_processed) /
                    static_cast<std::uint64_t>(stats.ticks);

        details.append("ticks=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.ticks)));

        details.append(" entities_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.entities_processed)));

        details.append(" avg_entities_per_tick=");
        details.append(std::to_string(avg_entities_per_tick));

        details.append(" scheduled_events=");
        details.append(std::to_string(core_event_count));

        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));

        details.append(" remaining_events=");
        details.append(std::to_string(scheduler.size()));

        details.append(" status_sweeps=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.status_sweeps)));

        details.append(" loop_duration_ms=");
        details.append(std::to_string(loop_duration_ms));

        details.append(" entities_per_second=");
        details.append(std::to_string(entities_per_second));

        details.append(" min_entities_per_second=");
        details.append(std::to_string(minimum_entities_per_second));
    }), read_env_u64(
        "MMO_BUDGET_WORLD_CORE_MS",
        stress_config.level == StressLevel::high ? 15000ull : 5000ull));

    push_result(run_test("stress.world_loop_status_sweeps", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        const std::uint32_t entity_count =
            std::min<std::uint32_t>(stress_config.world_entity_count, 100);

        const std::uint32_t max_ticks =
            std::min<std::uint32_t>(stress_config.world_max_ticks, 60);

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto poison_def = mmo::core::status::make_poison_definition();

            auto poison = mmo::core::status::make_instance(
                poison_def,
                now,
                1,
                entity_id);

            const auto applied = world.entities.apply_status(entity_id, poison);

            require(applied, "poison status was not applied");
        }

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = max_ticks;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        require_equal(
            static_cast<std::uint64_t>(max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "status world loop ticks");

        require(
            stats.status_sweeps > 0,
            "world loop did not process status sweeps even though poison statuses were applied");

        details.append("entities=");
        details.append(std::to_string(entity_count));
        details.append(" ticks=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.ticks)));
        details.append(" status_sweeps=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.status_sweeps)));
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
    }));

    push_result(run_test("stress.world_loop_empty_world", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = stress_config.world_max_ticks;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        require_equal(
            static_cast<std::uint64_t>(stress_config.world_max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "empty world loop ticks");

        require_equal(
            static_cast<std::uint64_t>(0),
            static_cast<std::uint64_t>(stats.entities_processed),
            "empty world entities processed");

        details.append("ticks=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.ticks)));
        details.append(" entities_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.entities_processed)));
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
        details.append(" status_sweeps=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.status_sweeps)));
    }));

    push_result(run_test("stress.world_loop_processes_due_events", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        auto& scheduler = world_event_scheduler(world);

        const auto now = mmo::core::time::now();

        const std::uint32_t event_count =
            stress_config.level == StressLevel::high ? 5000u : 500u;

        for (std::uint32_t i = 0; i < event_count; ++i)
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now;
            event.zone_id = static_cast<mmo::core::id::ZoneId>((i % 16) + 1);
            event.counter = i;

            require(
                scheduler.try_schedule(event),
                "world loop due event should schedule");
        }

        require_equal(
            static_cast<std::size_t>(event_count),
            scheduler.size(),
            "world event scheduler initial size");

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = 5;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        require_equal(
            static_cast<std::uint64_t>(config.max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "event world loop ticks");

        require_at_least(
            static_cast<std::uint64_t>(event_count),
            static_cast<std::uint64_t>(stats.events_processed),
            "world loop should process due events");

        require(
            scheduler.empty(),
            "world event scheduler should be empty after due events are processed");

        details.append("scheduled_due_events=");
        details.append(std::to_string(event_count));
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
        details.append(" remaining_events=");
        details.append(std::to_string(scheduler.size()));
    }));

    push_result(run_test("stress.world_loop_keeps_future_events_pending", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        auto& scheduler = world_event_scheduler(world);

        const auto now = mmo::core::time::now();

        constexpr std::uint32_t event_count = 1000;

        for (std::uint32_t i = 0; i < event_count; ++i)
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now + mmo::core::time::Milliseconds{ 3600000 };
            event.zone_id = static_cast<mmo::core::id::ZoneId>((i % 16) + 1);
            event.counter = i;

            require(
                scheduler.try_schedule(event),
                "future world event should schedule");
        }

        require_equal(
            static_cast<std::size_t>(event_count),
            scheduler.size(),
            "future event scheduler initial size");

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = 5;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        require_equal(
            static_cast<std::uint64_t>(config.max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "future event world loop ticks");

        require_equal(
            static_cast<std::uint64_t>(0),
            static_cast<std::uint64_t>(stats.events_processed),
            "future events should not be processed early");

        require_equal(
            static_cast<std::size_t>(event_count),
            scheduler.size(),
            "future events should remain pending");

        details.append("future_events=");
        details.append(std::to_string(event_count));
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
        details.append(" remaining_events=");
        details.append(std::to_string(scheduler.size()));
    }));

    push_result(run_test("stress.world_loop_mixed_entities_status_and_events", logger, [&](std::string& details) {
        mmo::world::World world{};
        auto catalog = build_item_catalog();

        auto& scheduler = world_event_scheduler(world);

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        const std::uint32_t entity_count =
            std::min<std::uint32_t>(stress_config.world_entity_count, 1000);

        const std::uint32_t event_count =
            std::min<std::uint32_t>(stress_config.event_count, 10000);

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.spawn_entity(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "mixed event world entity not found");

            if ((i % 2) == 0)
            {
                record->inventory = build_inventory(
                    catalog,
                    8,
                    1,
                    static_cast<std::uint64_t>(i) * 10000ull);

                mmo::core::entity::sync_load(*record, catalog);
            }

            if ((i % 3) == 0)
            {
                auto poison_def = mmo::core::status::make_poison_definition();
                auto poison = mmo::core::status::make_instance(poison_def, now, 1, entity_id);

                require(
                    world.entities.apply_status(entity_id, poison),
                    "mixed event world poison should apply");
            }
        }

        for (std::uint32_t i = 0; i < event_count; ++i)
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = now;
            event.zone_id = static_cast<mmo::core::id::ZoneId>((i % 16) + 1);
            event.counter = i;

            require(
                scheduler.try_schedule(event),
                "mixed world due event should schedule");
        }

        require_equal(
            static_cast<std::size_t>(event_count),
            scheduler.size(),
            "mixed world initial event count");

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = 60;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

        require_equal(
            static_cast<std::uint64_t>(config.max_ticks),
            static_cast<std::uint64_t>(stats.ticks),
            "mixed events world loop ticks");

        require_at_least(
            static_cast<std::uint64_t>(entity_count) * static_cast<std::uint64_t>(config.max_ticks),
            static_cast<std::uint64_t>(stats.entities_processed),
            "mixed events world loop entities processed");

        require_at_least(
            static_cast<std::uint64_t>(event_count),
            static_cast<std::uint64_t>(stats.events_processed),
            "mixed events world loop events processed");

        require(
            stats.status_sweeps > 0,
            "mixed events world loop should process status sweeps");

        require(
            scheduler.empty(),
            "mixed events world scheduler should be empty after due events");

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);
            auto* record = world.entities.find(entity_id);

            require(record != nullptr, "mixed events world entity missing after loop");

            require_inventory_invariants(
                catalog,
                record->inventory,
                "mixed events world inventory invariant");

            require(
                record->resources.health_current >= 0,
                "mixed events world entity hp below zero");

            require(
                record->resources.health_current <= record->stats.current_derived.max_hp,
                "mixed events world entity hp above max");
        }

        details.append("entities=");
        details.append(std::to_string(entity_count));
        details.append(" due_events=");
        details.append(std::to_string(event_count));
        details.append(" ticks=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.ticks)));
        details.append(" entities_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.entities_processed)));
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
        details.append(" status_sweeps=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.status_sweeps)));
    }));

    // =========================================================================
    // Summary output for console and structured logs.
    // =========================================================================

    const auto summary_header =
        format_summary_header(stress_config.level, results.size(), failures);

    logger.log(
        mmo::core::log::Level::info,
        "stress.summary",
        summary_header);

    std::cout << "Stress summary" << std::endl;
    std::cout << summary_header << std::endl;

    for (std::size_t index = 0; index < results.size(); ++index)
    {
        const auto& result = results[index];
        const auto summary_line = format_test_summary_line(index + 1, results.size(), result);

        logger.log(
            result.ok ? mmo::core::log::Level::info : mmo::core::log::Level::error,
            "stress.result",
            summary_line);

        std::cout << summary_line << std::endl;
    }

    std::cout << "Failures=" << failures << std::endl;

    logger.log(
        mmo::core::log::Level::info,
        "stress.summary",
        std::string("FINISH | failures=").append(std::to_string(failures)));

    return failures == 0 ? 0 : 1;
};
