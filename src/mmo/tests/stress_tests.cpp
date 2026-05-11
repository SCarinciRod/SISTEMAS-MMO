#include "mmo/mmo.hpp"
#include "mmo/server/loop.hpp"
#include "mmo/core/inventory.hpp"
#include "mmo/core/damage.hpp"
#include "mmo/core/event.hpp"
#include "mmo/core/material.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    // Test harness and stress configuration.
    struct TestResult
    {
        std::string name{};
        bool ok{ false };
        std::uint64_t duration_ms{ 0 };
        std::string details{};
    };

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

    auto parse_stress_level() -> StressLevel
    {
        const char* value = std::getenv("MMO_STRESS_LEVEL");
        if (value == nullptr)
        {
            return StressLevel::normal;
        }

        const std::string level{ value };
        if (level == "smoke")
        {
            return StressLevel::smoke;
        }

        if (level == "high")
        {
            return StressLevel::high;
        }

        if (level == "extreme")
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

    template <typename T>
    auto value_to_string(T value) -> std::string
    {
        if constexpr (std::is_enum_v<T>)
        {
            return std::to_string(static_cast<std::uint64_t>(value));
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
        {
            return std::to_string(static_cast<unsigned long long>(value));
        }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
        {
            return std::to_string(static_cast<long long>(value));
        }
        else
        {
            return std::to_string(value);
        }
    }

    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string(message));
        }
    }

    template <typename Expected, typename Actual>
    void require_equal(Expected expected, Actual actual, std::string_view label)
    {
        if (!(expected == actual))
        {
            std::string error;
            error.append(std::string(label));
            error.append(" expected=");
            error.append(value_to_string(expected));
            error.append(" actual=");
            error.append(value_to_string(actual));
            throw std::runtime_error(error);
        }
    }

    template <typename Minimum, typename Actual>
    void require_at_least(Minimum minimum, Actual actual, std::string_view label)
    {
        if (actual < minimum)
        {
            std::string error;
            error.append(std::string(label));
            error.append(" minimum=");
            error.append(value_to_string(minimum));
            error.append(" actual=");
            error.append(value_to_string(actual));
            throw std::runtime_error(error);
        }
    }

    template <typename Fn>
    auto run_test(std::string_view name, mmo::core::log::Logger& logger, Fn&& fn) -> TestResult
    {
        TestResult result{};
        result.name = std::string(name);

        const auto started_at = std::chrono::steady_clock::now();
        logger.log(mmo::core::log::Level::info, name, "start");

        try
        {
            fn(result.details);
            result.ok = true;
        }
        catch (const std::exception& ex)
        {
            result.details = ex.what();
            mmo::core::log::log_exception(logger, name, ex);
        }
        catch (...)
        {
            result.details = "unknown exception";
            mmo::core::log::log_exception(logger, name);
        }

        const auto finished_at = std::chrono::steady_clock::now();
        result.duration_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(finished_at - started_at).count());

        std::string message;
        message.append("end duration_ms=");
        message.append(std::to_string(result.duration_ms));

        if (!result.details.empty())
        {
            message.push_back(' ');
            message.append(result.details);
        }

        logger.log(
            result.ok ? mmo::core::log::Level::info : mmo::core::log::Level::error,
            name,
            message);

        return result;
    }

    // Item/material builders used by inventory/material test contracts.
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
            std::max<std::int32_t>(0, 100 - resistance_percent + packet.penetration_percent);

        const auto resolved_damage =
            (static_cast<std::int64_t>(packet.amount) * effective_percent) / 100;

        return static_cast<std::int32_t>(
            std::max<std::int64_t>(resolved_damage, 0));
    }

    // Combat helper that routes packets through the damage pipeline.
    auto apply_combat_damage_to_entity(
        mmo::core::runtime::World& world,
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

        return world.entities.apply_damage(target_entity_id, profile, mmo::core::time::now()).has_value();
    }
}

int main()
{
    mmo::core::log::Logger logger{};
    logger.set_file("stress.log");

    const auto stress_config = make_stress_config();

    logger.log(
        mmo::core::log::Level::info,
        "stress",
        std::string("selected_level=").append(std::string(stress_level_name(stress_config.level))));

    std::uint32_t failures = 0;
    std::vector<TestResult> results;

    auto push_result = [&](TestResult&& result) {
        failures += result.ok ? 0 : 1;
        results.push_back(std::move(result));
    };

    // Inventory contract tests.
    push_result(run_test("stress.inventory_weight_exact", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        auto inventory = build_inventory(
            catalog,
            stress_config.inventory_items_per_entity,
            10,
            0);

        const auto expected_once = expected_inventory_weight_from_instances(catalog, inventory);
        const auto actual_once = mmo::core::inventory::calculate_weight(catalog, inventory);

        require_equal(expected_once, static_cast<std::uint64_t>(actual_once), "inventory weight exact");

        std::uint64_t total = 0;

        for (std::uint32_t i = 0; i < stress_config.inventory_iterations; ++i)
        {
            const auto current = mmo::core::inventory::calculate_weight(catalog, inventory);
            require_equal(expected_once, static_cast<std::uint64_t>(current), "inventory weight changed across iterations");
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

        require_equal(static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(actual), "empty inventory weight");

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

        require_equal(expected, static_cast<std::uint64_t>(actual), "inventory weight with missing templates");

        details.append("expected_partial_weight=");
        details.append(std::to_string(expected));
        details.append(" actual_partial_weight=");
        details.append(std::to_string(static_cast<std::uint64_t>(actual)));
    }));

    push_result(run_test("stress.inventory_equipment_quantity_rule", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1));
        require(definition != nullptr, "equipment template not found");

        auto instance = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 999);
        instance.item_id = static_cast<mmo::core::id::ItemId>(1);

        require_equal(static_cast<std::uint32_t>(1), instance.quantity, "equipment quantity must be forced to 1");

        mmo::core::entity::Inventory inventory{};
        const auto add_result = mmo::core::inventory::add_item(catalog, inventory, instance);

        require(add_result.status == mmo::core::inventory::AddStatus::added, "equipment instance was not added");

        require_equal(static_cast<std::size_t>(1), inventory.items.size(), "equipment slot count");
        require_equal(static_cast<std::uint32_t>(1), inventory.items.front().quantity, "equipment inventory quantity");

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
            auto instance = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 1);
            instance.item_id = static_cast<mmo::core::id::ItemId>(i);

            const auto add_result = mmo::core::inventory::add_item(catalog, inventory, instance);

            require(
                add_result.status == mmo::core::inventory::AddStatus::added,
                "duplicated equipment template was not added as separate instance");
        }

        require_equal(static_cast<std::size_t>(10), inventory.items.size(), "duplicate equipment instance count");

        require_equal(
            static_cast<std::uint32_t>(10),
            mmo::core::inventory::count_template_quantity(
                inventory,
                static_cast<mmo::core::id::ItemTemplateId>(1)),
            "duplicate equipment template quantity");

        require(!mmo::core::inventory::has_duplicate_item_ids(inventory), "duplicated item_id inside duplicate equipment test");

        const auto validation = mmo::core::inventory::validate(catalog, inventory);
        require(validation == mmo::core::inventory::ValidationIssue::none, "duplicate equipment inventory validation failed");

        details.append("equipment_instances=10 same_template=true unique_ids=true");
    }));

    push_result(run_test("stress.inventory_material_stack_rule", logger, [&](std::string& details) {
        auto catalog = build_material_item_catalog();

        const auto* definition = catalog.find(static_cast<mmo::core::id::ItemTemplateId>(1001));
        require(definition != nullptr, "material template not found");

        mmo::core::entity::Inventory inventory{};

        auto first_stack = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 150);
        first_stack.item_id = static_cast<mmo::core::id::ItemId>(1001);

        auto second_stack = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 200);
        second_stack.item_id = static_cast<mmo::core::id::ItemId>(1002);

        const auto first_result = mmo::core::inventory::add_item(catalog, inventory, first_stack);
        require(first_result.status == mmo::core::inventory::AddStatus::added, "first material stack was not added");

        const auto second_result = mmo::core::inventory::add_item(catalog, inventory, second_stack);
        require(second_result.status == mmo::core::inventory::AddStatus::stacked, "second material stack was not merged into existing stack");

        require_equal(static_cast<std::size_t>(1), inventory.items.size(), "material stack slot count");
        require_equal(static_cast<std::uint32_t>(350), inventory.items.front().quantity, "material stack quantity");

        details.append("slots=1 quantity=350");
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
            auto instance = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 1);
            instance.item_id = static_cast<mmo::core::id::ItemId>(i);

            const auto result = mmo::core::inventory::add_item(catalog, inventory, instance);
            require(result.status == mmo::core::inventory::AddStatus::added, "item should fit before slot limit");
        }

        auto overflow = mmo::core::item::make_instance(*definition, mmo::core::time::now(), 1);
        overflow.item_id = static_cast<mmo::core::id::ItemId>(4);

        const auto overflow_result = mmo::core::inventory::add_item(catalog, inventory, overflow);

        require(overflow_result.status == mmo::core::inventory::AddStatus::full, "inventory did not reject item after slot limit");
        require_equal(static_cast<std::size_t>(3), inventory.items.size(), "slot limited inventory size");

        details.append("max_slots=3 rejected_status=");
        details.append(std::string(mmo::core::inventory::add_status_name(overflow_result.status)));
    }));

    push_result(run_test("stress.inventory_remove_item", logger, [&](std::string& details) {
        auto catalog = build_item_catalog();
        auto inventory = build_inventory(catalog, 10, 1, 2000000);

        require_equal(static_cast<std::size_t>(10), inventory.items.size(), "remove test initial inventory size");

        const auto remove_result = mmo::core::inventory::remove_item_by_id(
            inventory,
            static_cast<mmo::core::id::ItemId>(2000001),
            1,
            false);

        require(remove_result.status == mmo::core::inventory::RemoveStatus::removed, "inventory item was not removed");

        require_equal(static_cast<std::size_t>(9), inventory.items.size(), "remove test final inventory size");

        const auto missing_result = mmo::core::inventory::remove_item_by_id(
            inventory,
            static_cast<mmo::core::id::ItemId>(9999999),
            1,
            false);

        require(missing_result.status == mmo::core::inventory::RemoveStatus::not_found, "missing item removal should return not_found");

        details.append("before=10 after=9 missing_status=");
        details.append(std::string(mmo::core::inventory::remove_status_name(missing_result.status)));
    }));

    // Damage/material contract tests.
    push_result(run_test("stress.damage_kind_classification", logger, [&](std::string& details) {
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::slash), "slash should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::pierce), "pierce should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::sever), "sever should be physical");
        require(mmo::core::damage::is_physical(mmo::core::damage::Kind::impact), "impact should be physical");
        require(!mmo::core::damage::is_physical(mmo::core::damage::Kind::magic), "magic should not be physical");
        require(!mmo::core::damage::is_physical(mmo::core::damage::Kind::elemental), "elemental should not be physical");

        details.append("physical=4 non_physical=2");
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

    push_result(run_test("stress.material_damage_massive_map_objects", logger, [&](std::string& details) {
        const auto definition = make_map_material_definition();

        std::uint64_t total_damage = 0;

        for (std::uint32_t i = 0; i < stress_config.material_damage_iterations; ++i)
        {
            mmo::core::damage::Packet packet{};
            packet.kind = static_cast<mmo::core::damage::Kind>(i % 6);
            packet.amount = static_cast<std::int32_t>(50 + (i % 75));
            packet.penetration_percent = static_cast<std::int32_t>(i % 30);
            packet.can_ricochet = (i % 2) == 0;
            packet.can_embed = (i % 3) == 0;

            const auto expected = expected_material_damage(definition, packet);
            const auto actual = mmo::core::material::resolve_damage(definition, packet);

            require_equal(expected, actual, "massive material damage resolution");

            total_damage += static_cast<std::uint64_t>(actual);
        }

        require(total_damage > 0, "massive material damage produced zero total damage");

        details.append("iterations=");
        details.append(std::to_string(stress_config.material_damage_iterations));
        details.append(" total_damage=");
        details.append(std::to_string(total_damage));
    }));

    // Event contract tests.
    push_result(run_test("stress.event_validation_rules", logger, [&](std::string& details) {
        const auto now = mmo::core::time::now();

        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::evolution_due;
        event.due_at = now;

        require_equal(
            mmo::core::event::ValidationIssue::missing_entity,
            mmo::core::event::validate(event),
            "evolution_due requires entity");

        event.entity_id = static_cast<mmo::core::id::EntityId>(1);
        require(mmo::core::event::is_valid(event), "evolution_due valid");

        details = "validation_ok";
    }));

    push_result(run_test("stress.event_scheduler_basic", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        mmo::core::event::Event first{};
        first.type = mmo::core::event::Type::region_notice;
        first.due_at = now + mmo::core::time::Milliseconds{ 100 };
        first.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        first.counter = 1;

        mmo::core::event::Event second{};
        second.type = mmo::core::event::Type::region_notice;
        second.due_at = now + mmo::core::time::Milliseconds{ 50 };
        second.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        second.counter = 2;

        require(scheduler.try_schedule(first), "schedule first");
        require(scheduler.try_schedule(second), "schedule second");

        const auto ready = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 75 });

        require_equal(static_cast<std::size_t>(1), ready.size(), "ready count");
        require_equal(static_cast<std::uint32_t>(2), ready.front().counter, "ordering");

        details = "ordering_ok";
    }));

    push_result(run_test("stress.event_scheduler_ordering", logger, [&](std::string& details) {
        mmo::core::event::Scheduler scheduler{};
        const auto now = mmo::core::time::now();

        scheduler.schedule({ mmo::core::event::Type::region_notice, now + mmo::core::time::Milliseconds{ 300 }, {}, 3, 3 });
        scheduler.schedule({ mmo::core::event::Type::region_notice, now + mmo::core::time::Milliseconds{ 100 }, {}, 1, 1 });
        scheduler.schedule({ mmo::core::event::Type::region_notice, now + mmo::core::time::Milliseconds{ 200 }, {}, 2, 2 });

        const auto ready = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 250 });

        require_equal(static_cast<std::size_t>(2), ready.size(), "ready event count");
        require_equal(static_cast<std::uint32_t>(1), ready[0].counter, "first ready event counter");
        require_equal(static_cast<std::uint32_t>(2), ready[1].counter, "second ready event counter");
        require_equal(static_cast<std::size_t>(1), scheduler.size(), "remaining event count");

        const auto remaining = scheduler.pop_ready(now + mmo::core::time::Milliseconds{ 400 });

        require_equal(static_cast<std::size_t>(1), remaining.size(), "remaining ready event count");
        require_equal(static_cast<std::uint32_t>(3), remaining[0].counter, "remaining event counter");
        require(scheduler.empty(), "scheduler should be empty after all events popped");

        details.append("ordered_events=3");
    }));

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

    // Equipment affix aggregation and experience curve tests.
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

        require_equal(modifiers.attack_percent_delta, reversed_modifiers.attack_percent_delta, "affix attack order independence");
        require_equal(modifiers.defense_percent_delta, reversed_modifiers.defense_percent_delta, "affix defense order independence");
        require_equal(modifiers.move_speed_percent_delta, reversed_modifiers.move_speed_percent_delta, "affix move speed order independence");

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

        const auto accumulated_result = mmo::core::experience::gain(1, 0, 0, total_required);

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

    // Entity lifecycle and combat tracking tests.
    push_result(run_test("stress.entity_spawn_massive", logger, [&](std::string& details) {
        mmo::core::runtime::World world{};
        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        for (std::uint32_t i = 1; i <= stress_config.entity_spawn_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.entities.spawn(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "spawned entity was not found");
        }

        auto* missing = world.entities.find(
            static_cast<mmo::core::id::EntityId>(stress_config.entity_spawn_count + 1));

        require(missing == nullptr, "non-existent entity unexpectedly found");

        details.append("spawned_entities=");
        details.append(std::to_string(stress_config.entity_spawn_count));
    }));

    push_result(run_test("stress.combat_damage_multiple_entities", logger, [&](std::string& details) {
        mmo::core::runtime::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        std::uint64_t total_damage = 0;
        std::uint32_t killed = 0;
        std::uint32_t lethal_hits = 0;

        for (std::uint32_t i = 1; i <= stress_config.combat_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);
            world.entities.spawn(entity_id, blueprint, 1, now);

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

            require(apply_combat_damage_to_entity(world, entity_id, packet, entity_id), "damage application failed");

            auto* after = world.entities.find(entity_id);
            require(after != nullptr, "combat entity not found after damage");

            const auto expected_hp = std::clamp(hp_before - packet.amount, 0, before->stats.current_derived.max_hp);

            require_equal(expected_hp, after->resources.health_current, "multi entity damage hp");

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
        mmo::core::runtime::World world{};
        const auto blueprint = make_stress_blueprint(mmo::core::entity::Type::monster);
        const auto now = mmo::core::time::now();

        const auto target_id = static_cast<mmo::core::id::EntityId>(1);
        world.entities.spawn(target_id, blueprint, 1, now);

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

        require_equal(static_cast<std::int32_t>(expected_hp), target->resources.health_current, "single entity multi source hp");
        require_equal(static_cast<std::size_t>(stress_config.combat_sources_per_entity), damage_by_source.size(), "single entity source tracking count");

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
        mmo::core::runtime::World world{};
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
            world.entities.spawn(target_id, blueprint, 1, now);

            auto* record = world.entities.find(target_id);
            require(record != nullptr, "many target entity not found");

            expected_hp_by_entity[static_cast<std::uint64_t>(target_id)] = record->resources.health_current;
        }

        for (std::uint32_t target = 1; target <= stress_config.combat_entity_count; ++target)
        {
            const auto target_id = static_cast<mmo::core::id::EntityId>(target);

            for (std::uint32_t source = 1; source <= stress_config.combat_sources_per_entity; ++source)
            {
                for (std::uint32_t hit = 1; hit <= stress_config.combat_hits_per_source; ++hit)
                {
                    const auto amount = static_cast<std::int32_t>(1 + ((target + source + hit) % 3));

                    mmo::core::damage::Packet packet{};
                    packet.kind = static_cast<mmo::core::damage::Kind>((target + source + hit) % 6);
                    packet.amount = amount;

                    require(
                        apply_combat_damage_to_entity(
                            world,
                            target_id,
                            packet,
                            static_cast<mmo::core::id::EntityId>(source)),
                        "many target damage failed");

                    auto& expected_hp = expected_hp_by_entity[static_cast<std::uint64_t>(target_id)];
                    expected_hp = std::max<std::int64_t>(0, expected_hp - amount);

                    ++total_hits;
                    total_damage += static_cast<std::uint64_t>(amount);
                }
            }

            if (target_id == forced_kill_target_id)
            {
                auto& expected_hp = expected_hp_by_entity[static_cast<std::uint64_t>(target_id)];

                if (expected_hp > 0)
                {
                    mmo::core::damage::Packet lethal{};
                    lethal.kind = mmo::core::damage::Kind::impact;
                    lethal.amount = static_cast<std::int32_t>(expected_hp + 5);

                    require(
                        apply_combat_damage_to_entity(world, target_id, lethal, forced_source_id),
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
                static_cast<std::int32_t>(expected_hp_by_entity[static_cast<std::uint64_t>(target_id)]);

            require_equal(expected_hp, record->resources.health_current, "many source many entity hp");

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

    // World loop and status sweep tests.
    push_result(run_test("stress.world_loop_core", logger, [&](std::string& details) {
        mmo::core::runtime::World world{};
        auto catalog = build_item_catalog();

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        for (std::uint32_t i = 1; i <= stress_config.world_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.entities.spawn(entity_id, blueprint, 1, now);

            auto* record = world.entities.find(entity_id);
            require(record != nullptr, "spawned world entity was not found");

            record->inventory = build_inventory(
                catalog,
                stress_config.world_items_per_entity,
                10,
                static_cast<std::uint64_t>(i) * 100000ull);

            mmo::core::entity::sync_load(*record, catalog);

            auto poison_def = mmo::core::status::make_poison_definition();
            auto poison = mmo::core::status::make_instance(poison_def, now, 1, entity_id);

            world.entities.apply_status(entity_id, poison);
        }

        mmo::server::LoopConfig config{};
        config.tick_rate = stress_config.world_tick_rate;
        config.max_ticks = stress_config.world_max_ticks;
        config.sleep = false;

        const auto stats = mmo::server::run_loop(world, catalog, config, logger);

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

        for (std::uint32_t i = 1; i <= stress_config.world_entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);
            auto* record = world.entities.find(entity_id);

            require(record != nullptr, "entity missing after world loop");

            require_equal(
                static_cast<std::size_t>(stress_config.world_items_per_entity),
                record->inventory.items.size(),
                "inventory item count changed after world loop");

            const auto inventory_validation = mmo::core::inventory::validate(catalog, record->inventory);

            require(
                inventory_validation == mmo::core::inventory::ValidationIssue::none,
                "inventory validation failed after world loop");
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
        details.append(" events_processed=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.events_processed)));
        details.append(" status_sweeps=");
        details.append(std::to_string(static_cast<std::uint64_t>(stats.status_sweeps)));
    }));

    push_result(run_test("stress.world_loop_status_sweeps", logger, [&](std::string& details) {
        mmo::core::runtime::World world{};
        auto catalog = build_item_catalog();

        const auto blueprint = make_stress_blueprint();
        const auto now = mmo::core::time::now();

        const std::uint32_t entity_count = std::min<std::uint32_t>(stress_config.world_entity_count, 100);
        const std::uint32_t max_ticks = std::min<std::uint32_t>(stress_config.world_max_ticks, 60);

        for (std::uint32_t i = 1; i <= entity_count; ++i)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(i);

            world.entities.spawn(entity_id, blueprint, 1, now);

            auto poison_def = mmo::core::status::make_poison_definition();
            auto poison = mmo::core::status::make_instance(poison_def, now, 1, entity_id);

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

        require(stats.status_sweeps > 0, "world loop did not process status sweeps even though poison statuses were applied");

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
        mmo::core::runtime::World world{};
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

    // Summary output for console and structured logs.
    logger.log(
        mmo::core::log::Level::info,
        "stress.summary",
        std::string("level=").append(std::string(stress_level_name(stress_config.level))));

    std::cout << "Stress summary" << std::endl;
    std::cout << "Level=" << stress_level_name(stress_config.level) << std::endl;

    for (const auto& result : results)
    {
        std::string summary_line = result.ok ? "PASS " : "FAIL ";
        summary_line.append(result.name);
        summary_line.append(" duration_ms=");
        summary_line.append(std::to_string(result.duration_ms));

        logger.log(
            result.ok ? mmo::core::log::Level::info : mmo::core::log::Level::error,
            "stress.summary",
            summary_line + (result.details.empty() ? "" : " " + result.details));

        std::cout << (result.ok ? "PASS" : "FAIL")
                  << " " << result.name
                  << " duration_ms=" << result.duration_ms;

        if (!result.details.empty())
        {
            std::cout << " " << result.details;
        }

        std::cout << std::endl;
    }

    std::cout << "Failures=" << failures << std::endl;

    logger.log(
        mmo::core::log::Level::info,
        "stress.summary",
        std::string("failures=").append(std::to_string(failures)));

    return failures == 0 ? 0 : 1;
}
