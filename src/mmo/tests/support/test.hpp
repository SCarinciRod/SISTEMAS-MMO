#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mmo
{
    namespace tests
    {
        namespace support
        {
            struct TestResult
            {
                std::string name{};
                bool ok{ false };
                std::uint64_t duration_ms{ 0 };
                std::string details{};
                std::uint64_t performance_budget_ms{ 0 };
                bool budget_checked{ false };
            };

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
                else if constexpr (std::is_convertible_v<T, std::string_view>)
                {
                    return std::string(std::string_view(value));
                }
                else
                {
                    return std::to_string(value);
                }
            }

            inline auto require(bool condition, std::string_view message) -> void
            {
                if (!condition)
                {
                    throw std::runtime_error(std::string(message));
                }
            }

            template <typename Expected, typename Actual>
            auto require_equal(Expected expected, Actual actual, std::string_view label) -> void
            {
                if (expected == actual)
                {
                    return;
                }

                std::string error{ label };
                error.append(" expected=");
                error.append(value_to_string(expected));
                error.append(" actual=");
                error.append(value_to_string(actual));
                throw std::runtime_error(error);
            }

            template <typename Minimum, typename Actual>
            auto require_at_least(Minimum minimum, Actual actual, std::string_view label) -> void
            {
                if (actual >= minimum)
                {
                    return;
                }

                std::string error{ label };
                error.append(" minimum=");
                error.append(value_to_string(minimum));
                error.append(" actual=");
                error.append(value_to_string(actual));
                throw std::runtime_error(error);
            }

            template <typename Maximum, typename Actual>
            auto require_at_most(Maximum maximum, Actual actual, std::string_view label) -> void
            {
                if (actual <= maximum)
                {
                    return;
                }

                std::string error{ label };
                error.append(" maximum=");
                error.append(value_to_string(maximum));
                error.append(" actual=");
                error.append(value_to_string(actual));
                throw std::runtime_error(error);
            }

            template <typename Fn>
            auto run_test(std::string_view name, Fn&& fn) -> TestResult
            {
                TestResult result{};
                result.name = std::string(name);
                const auto started_at = std::chrono::steady_clock::now();

                try
                {
                    fn(result.details);
                    result.ok = true;
                }
                catch (const std::exception& ex)
                {
                    result.details = ex.what();
                }
                catch (...)
                {
                    result.details = "unknown exception";
                }

                const auto finished_at = std::chrono::steady_clock::now();
                result.duration_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        finished_at - started_at).count());
                return result;
            }

            inline auto format_test_summary_line(
                std::size_t index,
                std::size_t total,
                const TestResult& result) -> std::string
            {
                std::string message;
                message.reserve(result.name.size() + result.details.size() + 160);
                message.append(std::to_string(index));
                message.push_back('/');
                message.append(std::to_string(total));
                message.append(" | ");
                message.append(result.ok ? "PASS" : "FAIL");
                message.append(" | ");
                message.append(result.name);
                message.append(" | duration=");
                message.append(std::to_string(result.duration_ms));
                message.append("ms");

                if (result.budget_checked)
                {
                    message.append(" | budget=");
                    message.append(std::to_string(result.performance_budget_ms));
                    message.append("ms");
                }

                if (!result.details.empty())
                {
                    message.append(" | ");
                    message.append(result.details);
                }

                return message;
            }

            inline auto report_results(
                std::string_view suite_name,
                const std::vector<TestResult>& results,
                std::ostream& output) -> int
            {
                std::size_t failures = 0;
                output << suite_name << " summary\n";

                for (std::size_t index = 0; index < results.size(); ++index)
                {
                    output << format_test_summary_line(index + 1, results.size(), results[index]) << '\n';
                    failures += results[index].ok ? 0 : 1;
                }

                output << "Failures=" << failures << '\n';
                return failures == 0 ? 0 : 1;
            }
        }
    }
}
