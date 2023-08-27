#pragma once

#include <charconv>
#include <expected>
#include <string>

#include "CommandExecutor.h"

template<typename T> requires (std::is_trivially_copyable_v<T>)
std::expected<T, std::string> parse(std::string_view);;

template<>
std::expected<int, std::string> parse<int>(std::string_view string) {
    int out{};
    auto [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), out);
    if (ec == std::errc()) {
        return out;
    } else {
        return std::unexpected(std::string{"Failed to parse \""}.append(string).append("\" as an integer"));
    }
}

template<>
std::expected<std::string_view, std::string> parse<std::string_view>(std::string_view string) {
    return string;
}
