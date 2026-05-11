#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "core/logger.hpp"

namespace mmo
{
    namespace persistence
    {
        enum class Backend : std::uint8_t
        {
            none = 0,
            sqlite
        };

        struct ContextConfig
        {
            Backend backend{ Backend::none };
            std::string base_path{ "data" };
            bool create_directories{ true };
        };

        struct Context
        {
            ContextConfig config{};
            bool ready{ false };
            std::string last_error{};
        };

        [[nodiscard]] inline auto initialize(Context& context, const ContextConfig& config, core::log::Logger& logger) -> bool
        {
            context.config = config;
            context.ready = false;
            context.last_error.clear();

            try
            {
                if (config.create_directories && !config.base_path.empty())
                {
                    std::filesystem::create_directories(config.base_path);
                }

                context.ready = true;
                return true;
            }
            catch (const std::exception& ex)
            {
                context.last_error = ex.what();
                core::log::log_exception(logger, "persistence.initialize", ex);
                return false;
            }
            catch (...)
            {
                context.last_error = "unknown error";
                core::log::log_exception(logger, "persistence.initialize");
                return false;
            }
        }

        inline auto shutdown(Context& context) -> void
        {
            context.ready = false;
        }
    }
}
