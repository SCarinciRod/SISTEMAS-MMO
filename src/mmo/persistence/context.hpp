#pragma once

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

#include "mmo/core/logger.hpp"

namespace mmo
{
    namespace persistence
    {
        enum class Backend : std::uint8_t
        {
            none = 0,
            filesystem,
            sqlite
        };

        struct ContextConfig
        {
            Backend backend{ Backend::none };

            // Base directory used by filesystem-like backends.
            // For Backend::none this value is ignored.
            std::string base_path{ "data" };

            // If true, initialize() creates base_path when needed.
            // If false, initialize() requires base_path to already exist.
            bool create_directories{ true };
        };

        struct Context
        {
            ContextConfig config{};
            bool ready{ false };
            std::string last_error{};
        };

        [[nodiscard]] inline auto backend_name(Backend backend) -> std::string_view
        {
            switch (backend)
            {
                case Backend::none:
                    return "none";

                case Backend::filesystem:
                    return "filesystem";

                case Backend::sqlite:
                    return "sqlite";
            }

            return "unknown";
        }

        inline auto set_context_error(
            Context& context,
            core::log::Logger& logger,
            std::string message) -> bool
        {
            context.ready = false;
            context.last_error = std::move(message);

            logger.log(
                core::log::Level::error,
                "persistence.context",
                context.last_error);

            return false;
        }

        [[nodiscard]] inline auto validate_filesystem_base_path(
            const ContextConfig& config,
            Context& context,
            core::log::Logger& logger) -> bool
        {
            if (config.base_path.empty())
            {
                return set_context_error(
                    context,
                    logger,
                    "filesystem backend requires a non-empty base_path");
            }

            const std::filesystem::path base_path{ config.base_path };

            if (std::filesystem::exists(base_path))
            {
                if (!std::filesystem::is_directory(base_path))
                {
                    return set_context_error(
                        context,
                        logger,
                        "filesystem backend base_path exists but is not a directory: " + config.base_path);
                }

                return true;
            }

            if (!config.create_directories)
            {
                return set_context_error(
                    context,
                    logger,
                    "filesystem backend base_path does not exist and create_directories is false: " + config.base_path);
            }

            std::filesystem::create_directories(base_path);

            if (!std::filesystem::exists(base_path) || !std::filesystem::is_directory(base_path))
            {
                return set_context_error(
                    context,
                    logger,
                    "filesystem backend failed to create base_path: " + config.base_path);
            }

            return true;
        }

        [[nodiscard]] inline auto initialize(
            Context& context,
            const ContextConfig& config,
            core::log::Logger& logger) -> bool
        {
            context.config = config;
            context.ready = false;
            context.last_error.clear();

            try
            {
                switch (config.backend)
                {
                    case Backend::none:
                    {
                        // Explicit no-persistence mode.
                        // This is useful for tests, tools, and server modes where persistence is disabled.
                        context.ready = true;

                        logger.log(
                            core::log::Level::info,
                            "persistence.context",
                            "initialized backend=none");

                        return true;
                    }

                    case Backend::filesystem:
                    {
                        if (!validate_filesystem_base_path(config, context, logger))
                        {
                            return false;
                        }

                        context.ready = true;

                        logger.log(
                            core::log::Level::info,
                            "persistence.context",
                            "initialized backend=filesystem base_path=" + config.base_path);

                        return true;
                    }

                    case Backend::sqlite:
                    {
                        return set_context_error(
                            context,
                            logger,
                            "sqlite backend not implemented");
                    }
                }

                return set_context_error(
                    context,
                    logger,
                    "unknown persistence backend");
            }
            catch (const std::exception& ex)
            {
                context.ready = false;
                context.last_error = ex.what();

                core::log::log_exception(
                    logger,
                    "persistence.initialize",
                    ex);

                return false;
            }
            catch (...)
            {
                context.ready = false;
                context.last_error = "unknown error";

                core::log::log_exception(
                    logger,
                    "persistence.initialize");

                return false;
            }
        }

        inline auto shutdown(Context& context) -> void
        {
            context.ready = false;
        }

        [[nodiscard]] inline auto is_ready(const Context& context) -> bool
        {
            return context.ready;
        }
    }
}
