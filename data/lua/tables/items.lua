-- Example items table for prototyping the Lua loader
-- The script returns a table keyed by numeric ItemTemplateId
return {
    [1001] = {
        name = "Short Sword",
        kind = "equipment",
        equipment = {
            class = "weapon",
            slot = "main_hand",
            attack = 12,
            defense = 0,
            weight = 8,
            two_handed = false,
            max_durability = 10,
            modification_slots = 2,
            affix_slots = 3,
            requirements = {
                minimum_primary = {
                    strength = 10,
                    dexterity = 6,
                },
                under_requirement_penalty_percent = 25,
            },
        },
    },

    [1002] = {
        name = "Iron Ring",
        kind = "equipment",
        equipment = {
            class = "accessory",
            slot = "ring_1",
            attack = 0,
            defense = 0,
            weight = 1,
            two_handed = false,
            max_durability = 15,
            modification_slots = 1,
            affix_slots = 1,
            requirements = {
                minimum_primary = {
                    faith = 5,
                },
                under_requirement_penalty_percent = 10,
            },
        },
    },

    [2001] = {
        name = "Health Potion",
        kind = "consumable",
        consumable = {
            consumed_on_use = true,
        },
    },
}
