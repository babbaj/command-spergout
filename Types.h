#pragma once

#include <charconv>
#include <expected>
#include <string>

#include "CommandExecutor.h"

template<typename T>
std::expected<T, std::string> parse(std::string_view);

template<typename T> requires std::is_arithmetic_v<T>
std::expected<T, std::string> parse(std::string_view string) {
    T out{};
    auto [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), out);
    if (ec == std::errc()) {
        return out;
    } else {
        return std::unexpected(std::string{"Failed to parse \""}.append(string).append("\" as an ").append(std::is_integral_v<T> ? "integer" : "float"));
    }
}

template<>
std::expected<std::string_view, std::string> parse<std::string_view>(std::string_view string) {
    return string;
}
