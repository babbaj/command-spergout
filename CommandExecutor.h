#pragma once
#include <optional>
#include <string>

#include "Command.h"
#include "Types.h"

struct Lexer {
    struct UnbalancedQuoteError {};

    std::string_view input;
    std::expected<std::optional<std::string_view>, UnbalancedQuoteError> next_result{};

    explicit Lexer(std::string_view in): input(in) {
        consume_arg();
    }

    std::expected<std::optional<std::string_view>, UnbalancedQuoteError> next() {
        auto out = next_result;
        consume_arg();
        return out;
    }

    void consume_arg() {
        auto expect = parse();
        if (expect.has_value()) {
            auto next = expect.value();
            if (next) {
                auto &[word, rest] = *next;
                input = rest;
                next_result = word;
            } else {
                next_result.emplace();
            }
        } else {
            next_result = std::unexpected(UnbalancedQuoteError{});
        }
    }

    [[nodiscard]] std::expected<std::optional<std::string_view>, UnbalancedQuoteError> peek() const {
        return next_result;
    }

    // returns the next argument and the remaining input
    [[nodiscard]] std::expected<std::optional<std::pair<std::string_view, std::string_view>>, UnbalancedQuoteError> parse() const {
        for (int i = 0; i < input.size(); i++) {
            if (input[i] == ' ') continue;
            if (input[i] == '"') {
                for (int j = i + 1; j < input.size(); j++) {
                    if (input[j] == '"') {
                        return {{{
                            {&input[i + 1], &input[j]},
                            {&input[j + 1], input.end()}
                        }}};
                    }
                }
                return std::unexpected(UnbalancedQuoteError{});
            }
            for (int j = i; j < input.size(); j++) {
                if (input[j] == ' ') {
                    return {{{
                        {&input[i], &input[j]},
                        {&input[j + 1], input.end()}
                    }}};
                }
            }
            return {{{
                {&input[i], input.end()},
                {}
            }}};
        }
        // no more tokens
        return {};
    }
};

template<typename... Overloads>
consteval auto overload_arg_counts() {
    auto out = std::array<int, sizeof...(Overloads)>{Overloads::num_args...};
    std::sort(out.begin(), out.end());
    return out;
}

template<int argc, typename Overload, typename... Rest>
auto find_overload_for_arg_count() {
    if constexpr (argc == Overload::num_args) {
        return std::type_identity<Overload>{};
    } else {
        return find_overload_for_arg_count<argc, Rest...>();
    }
}

/*template<typename... Args>
std::optional<std::tuple<>> build_arguments(Brig2::ArgumentSpec<Args...>) {
    return {{}};
}

template<typename Arg, typename... Rest>
std::optional<std::tuple<typename Arg::Type, typename Rest::Type...>> build_arguments(Brig2::ArgumentSpec<Arg, Rest...>, std::string_view s, auto... strings) requires (sizeof...(Rest) == (sizeof...(strings))) {
    auto parsed = Parsable<typename Arg::Type>{s}.parse();
    if (!parsed) return {};
    auto next = build_arguments(Brig2::ArgumentSpec<Rest...>{}, strings...);
    if (!next) return {};
    return std::tuple_cat(std::make_tuple(*parsed), *next);
}*/

template<size_t I, typename Arg, typename... Rest>
std::expected<void, std::string> set_arguments(auto& tuple, std::string_view s, auto... strings) requires (sizeof...(Rest) == (sizeof...(strings))) {
    auto parsed = parse<typename Arg::Type>(s);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    std::get<I>(tuple) = *parsed;
    if constexpr (sizeof...(strings) == 0) {
        return {};
    } else {
        return set_arguments<I + 1, Rest...>(tuple, strings...);
    }
}

template<typename... Args>
std::expected<std::tuple<typename Args::Type...>, std::string> build_arguments2(Brig2::ArgumentSpec<Args...>, auto... strings) requires (sizeof...(Args) == (sizeof...(strings))) {
    std::expected<std::tuple<typename Args::Type...>, std::string> out{};
    if constexpr (sizeof...(strings) > 0) {
        auto err = set_arguments<0, Args...>(out.value(), strings...);
        if (!err.has_value()) {
            return std::unexpected(std::move(err.error()));
        } else {
            return out;
        }
    } else {
        return {};
    }
}

template<typename Callback, typename... Arg>
std::expected<void, std::string> invoke_callback(Callback& cb, Brig2::ArgumentSpec<Arg...> spec, auto... strings) requires (sizeof...(Arg) == sizeof...(strings)) {
    auto parsed = build_arguments2(spec, strings...);
    if (!parsed) {
        return std::unexpected(std::move(parsed.error()));
    }
    std::apply(cb, *parsed);
    return {};
}

template<hat::string_literal Name, typename Callback, typename... Overloads> requires (sizeof...(Overloads) > 0)
std::expected<void, std::string> find_and_invoke_callback(Lexer lexer, Brig2::Command<Name, Callback, Overloads...>& cmd, auto... strings) {
    constexpr std::array meme = overload_arg_counts<Overloads...>();
    constexpr int maxArgCount = *std::max_element(meme.begin(), meme.end());
    static_assert(sizeof...(strings) <= maxArgCount);
    constexpr bool overloadExistsForArgs = std::find(meme.begin(), meme.end(), sizeof...(strings)) != meme.end();
    if constexpr (overloadExistsForArgs) {
        using Overload = decltype(find_overload_for_arg_count<sizeof...(strings), Overloads...>())::type;
        auto peek_expected = lexer.peek();
        if (!peek_expected.has_value()) {
            return std::unexpected("unbalanced quotes");
        }
        if (!peek_expected.value().has_value()) {
            return invoke_callback(cmd.callback, Overload{}, strings...);
        }
    }
    if constexpr (sizeof...(strings) < maxArgCount) {
        auto next_expected = lexer.next();
        if (!next_expected.has_value()) {
            return std::unexpected("unbalanced quotes");
        }
        auto next = next_expected.value();
        if (next) {
            auto err = find_and_invoke_callback(lexer, cmd, strings..., *next);
            return err;
        } else {
            return std::unexpected(std::string{"no overload with "} + std::to_string(sizeof...(strings)) + " args");
        }
    } else {
        return std::unexpected("too many arguments given to command");
    }
}

template<typename Cmd, typename... ChildTrees>
std::expected<bool, std::string> execute_recurse(Lexer lexer, Brig2::CommandTree<Cmd, ChildTrees...>& tree) {
    auto lexer_err = lexer.next();
    if (!lexer_err.has_value()) {
        return std::unexpected("Unbalanced quotes");
    }
    std::optional<std::string_view> arg = lexer_err.value();
    if (!arg || *arg != tree.command.getName()) return false;

    if constexpr (sizeof...(ChildTrees) == 0) {
        auto err = find_and_invoke_callback(lexer, tree.command);
        if (err) {
            return true;
        } else {
            return std::unexpected(std::move(err.error()));
        }
    } else {
        std::expected<bool, std::string> err{};
        (void)((err = execute_recurse(lexer, std::get<ChildTrees>(tree.children)), (!err.has_value() || err.value())) || ...);
        return err;
    }
}
