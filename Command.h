#pragma once
#include <tuple>
#include <utility>
#include <array>

#include "libhat/include/libhat/CompileTime.hpp"

namespace Brig2 {
    template<hat::string_literal identifier, typename T>
    struct Argument {
        using Type = T;
        static constexpr auto Identifier = identifier;
        static constexpr auto UsageString = hat::string_literal{"<"} + Identifier + ">";
    };

    template<typename... Args> //requires (std::derived_from<Args, Argument> && ...)
    struct ArgumentSpec {
        static constexpr auto num_args = sizeof...(Args);
    };

    template<typename>
    constexpr auto isArgSpec = false;
    template<typename... Args>
    constexpr auto isArgSpec<ArgumentSpec<Args...>> = true;

    // xor should mean the same as != but not here?
    // A tree is valid if it has a callback that takes 0 arguments, has 0 overloads but some children, or some argument accepting overloads and 0 children.
    // Potentially ambiguous trees or useless trees are invalid.
    template<typename Cmd, typename... ChildTrees>
    constexpr bool isTreeValid = ((Cmd::has0ArgOverload && Cmd::num_overloads == 1) || (Cmd::num_overloads == 0 ^ sizeof...(ChildTrees) == 0));

    template<typename Cmd, typename... ChildTrees> requires (isTreeValid<Cmd, ChildTrees...> && (std::is_object_v<ChildTrees> && ...))
    struct CommandTree {
        using CmdType = Cmd;
        Cmd command;
        std::tuple<ChildTrees...> children;

        CommandTree _intoTree() && {
            return std::move(*this);
        }
    };

    template<hat::string_literal Name, typename Callback, typename... Overloads> requires (isArgSpec<Overloads> && ...)
    struct Command {
        static constexpr auto name = Name;
        static constexpr int num_overloads = sizeof...(Overloads);
        static constexpr bool has0ArgOverload = (... || (Overloads::num_args == 0));
        Callback callback;

        constexpr std::string_view getName() {
            return Name.to_view();
        }

        Command _intoCommand() && {
            return std::move(*this);
        }
    };


    template<typename Cmd, typename... Children>
    constexpr auto Tree(Cmd&& cmd, Children&&... children) requires (
            // rvalue references only
            std::is_rvalue_reference_v<Cmd&&> && (std::is_rvalue_reference_v<Children&&> && ...)
            // a command with no overloads or children has no reason to exist
            && (Cmd::num_overloads > 0 || sizeof...(Children) > 0))
    {
        // clang can't deduce types here but gcc can
        return CommandTree<decltype(std::move(cmd)._intoCommand()), Children...>{
            std::move(cmd)._intoCommand(), std::make_tuple(std::move(children)...)
        };
    }

    template<class... Ts>
    struct overloaded : Ts... { using Ts::operator()...; };
    // technically not needed but clang is cranky
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    template<typename... Ts>
    constexpr auto tuple_callback(std::tuple<Ts...>&& tuple) {
        return std::apply([](auto&&... elems) {
            return overloaded { std::move(elems)... };
        }, tuple);
    }

    template<typename Tuple, typename More>
    struct tuple_append;
    template<typename More, typename... Ts>
    struct tuple_append<std::tuple<Ts...>, More> : std::type_identity<std::tuple<Ts..., More>> {};

    template<hat::string_literal Name, typename CallbackTuple, typename... Overloads> requires (isArgSpec<Overloads> && ...)
    struct CommandBuilder {
        static constexpr int num_overloads = sizeof...(Overloads);
        CallbackTuple callbacks;
        template<typename... CmdArgs, typename F>
        constexpr CommandBuilder<Name, typename tuple_append<CallbackTuple, F>::type, Overloads..., ArgumentSpec<CmdArgs...>> Accepts(F cb) {
            return {std::tuple_cat(std::move(this->callbacks), std::tuple<F>{std::move(cb)})};
        }

        constexpr Command<Name, decltype(tuple_callback(std::declval<CallbackTuple>())), Overloads...> Build() {
            return {tuple_callback(std::move(this->callbacks))};
        }

        constexpr auto _intoCommand() && {
            return this->Build();
        }

        template<typename... Children> requires (/*sizeof...(Overloads) == 0 &&*/ (std::is_rvalue_reference_v<Children&&> && ...))
        constexpr auto WithChildren(Children&&... children) {
            return Tree(this->Build(), std::move(children)._intoTree()...);
        }

        auto _intoTree() && {
            return Tree(this->Build());
        }
    };

    template<hat::string_literal literal>
    constexpr CommandBuilder<literal, std::tuple<>> Literal() {
        return {};
    }

    template<typename Arg, typename... Rest>
    constexpr auto CatArgUsage(ArgumentSpec<Arg, Rest...>) {
        if constexpr (sizeof...(Rest) == 0) {
            return Arg::UsageString;
        } else {
            return Arg::UsageString + ((hat::string_literal{" "} + Rest::UsageString) + ...);
        }
    }
    constexpr auto CatArgUsage(ArgumentSpec<>) {
        return hat::string_literal{""};
    }

    template<hat::string_literal Name, typename CB, typename... Overloads>
    constexpr auto BuildCommandUsage0(std::type_identity<Command<Name, CB, Overloads...>>) {
        return std::make_tuple(Name + " " + CatArgUsage(Overloads{})...);
    }

    template<typename Cmd, typename... ChildTrees>
    constexpr auto BuildCommandUsage0(std::type_identity<CommandTree<Cmd, ChildTrees...>>) {
        using TreeType = CommandTree<Cmd, ChildTrees...>;
        if constexpr (Cmd::num_overloads > 0) {
            return BuildCommandUsage0(std::type_identity<typename TreeType::CmdType>{});
        } else {
            auto postfix = std::tuple_cat(BuildCommandUsage0(std::type_identity<ChildTrees>{})...);
            auto withPrefix = std::apply([](auto... str) {
                return std::make_tuple((Cmd::name + " " + str)...);
            }, postfix);
            return withPrefix;
        }
    }

    template<typename Tree>
    constexpr auto CommandUsageTuple = BuildCommandUsage0(std::type_identity<Tree>{});
    template<typename Tree>
    constexpr auto CommandUsage = []<size_t... I>(std::index_sequence<I...>) {
        return std::array{std::get<I>(CommandUsageTuple<Tree>).to_view()...};
    }(std::make_index_sequence<std::tuple_size_v<decltype(CommandUsageTuple<Tree>)>>{});

    template<typename Cmd, typename... ChildTrees>
    constexpr const auto& BuildCommandUsage(const CommandTree<Cmd, ChildTrees...>&) {
        return CommandUsage<CommandTree<Cmd, ChildTrees...>>;
    }
}
