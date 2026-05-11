#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace mmo
{
    namespace core
    {
        namespace log
        {
            enum class Level : std::uint8_t
            {
                trace = 0,
                debug,
                info,
                warn,
                error,
                critical
            };

            [[nodiscard]] inline auto level_name(Level level) -> std::string_view
            {
                switch (level)
                {
                    case Level::trace:
                        return "trace";
                    case Level::debug:
                        return "debug";
                    case Level::info:
                        return "info";
                    case Level::warn:
                        return "warn";
                    case Level::error:
                        return "error";
                    case Level::critical:
                        return "critical";
                    default:
                        return "unknown";
                }
            }

            class Logger
            {
            public:
                Logger() = default;

                explicit Logger(std::ostream& output)
                    : output_(&output)
                {
                }

                auto set_output(std::ostream& output) -> void
                {
                    output_ = &output;
                }

                auto set_file(std::string_view path, bool tee_to_stderr = true) -> bool
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    file_.open(std::string(path), std::ios::app);
                    tee_to_stderr_ = tee_to_stderr;
                    return file_.is_open();
                }

                auto log(Level level, std::string_view message) -> void
                {
                    log(level, {}, message);
                }

                auto log(Level level, std::string_view context, std::string_view message) -> void
                {
                    const auto now = std::chrono::steady_clock::now();
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
                    const auto sequence = sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
                    const auto thread_id = std::this_thread::get_id();

                    std::lock_guard<std::mutex> lock(mutex_);
                    if (file_.is_open())
                    {
                        write_line(file_, level, elapsed, sequence, thread_id, context, message);
                        file_.flush();
                    }

                    if (tee_to_stderr_ && output_ != nullptr)
                    {
                        write_line(*output_, level, elapsed, sequence, thread_id, context, message);
                        output_->flush();
                    }
                }

            private:
                std::mutex mutex_{};
                std::ostream* output_{ &std::cerr };
                std::ofstream file_{};
                bool tee_to_stderr_{ true };
                std::chrono::steady_clock::time_point start_{ std::chrono::steady_clock::now() };
                std::atomic<std::uint64_t> sequence_{ 0 };

                auto write_line(
                    std::ostream& stream,
                    Level level,
                    std::int64_t elapsed_ms,
                    std::uint64_t sequence,
                    std::thread::id thread_id,
                    std::string_view context,
                    std::string_view message) -> void
                {
                    stream << "[" << elapsed_ms << "ms]"
                           << "[#" << sequence << "]"
                           << "[" << level_name(level) << "]"
                           << "[tid=" << thread_id << "]";

                    if (!context.empty())
                    {
                        stream << "[" << context << "]";
                    }

                    stream << " " << message << "\n";
                }
            };

            [[nodiscard]] inline auto default_logger() -> Logger&
            {
                static Logger logger{};
                return logger;
            }

            inline auto log_exception(Logger& logger, std::string_view context, const std::exception& ex) -> void
            {
                std::string message;
                message.reserve(context.size() + 64);
                message.append("exception in ");
                message.append(context);
                message.append(": ");
                message.append(ex.what());
                logger.log(Level::error, context, message);
            }

            inline auto log_exception(Logger& logger, std::string_view context) -> void
            {
                std::string message;
                message.reserve(context.size() + 48);
                message.append("unknown exception in ");
                message.append(context);
                logger.log(Level::critical, context, message);
            }
        }
    }
}
