#pragma once

#include <charconv>
#include <expected>
#include <string>

#include "CommandExecutor.h"

template<typename T> requires (std::is_trivially_copyable_v<T>)
struct Parsable {
    std::string_view string;

    std::expected<T, std::string> parse();
};

template<>
std::expected<int, std::string> Parsable<int>::parse() {
    int out{};
    auto [ptr, ec] = std::from_chars(this->string.data(), this->string.data() + this->string.size(), out);
    if (ec == std::errc()) {
        return out;
    } else {
        return std::unexpected(std::string{"Failed to parse \""}.append(this->string).append("\" as an integer"));
    }
}

template<>
std::expected<std::string_view, std::string> Parsable<std::string_view>::parse() {
    return this->string;
}
