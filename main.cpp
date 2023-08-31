#include <iostream>

#ifdef __clang__
#undef __cpp_concepts
#define __cpp_concepts 202002L
#endif
#include "CommandNode.hpp"
#include "Command.h"
#include "CommandExecutor.h"


int main(int argc, char** argv) {
    using namespace Brig2;

    auto tree =
        Literal<"elytra">()
                .WithChildren(
                        Literal<"repack">().Accepts([argc] {
                            std::cout << "invoked repack" << std::endl;
                        }),
                        Literal<"reset">().Accepts([] {
                            std::cout << "invoked reset" << std::endl;
                        }).WithChildren(
                                Literal<"meow">().Accepts([]{
                                    std::cout << "invoked meow" << std::endl;
                                })
                        ),
                        Literal<"test">().WithChildren(
                            Literal<"sneed">().Accepts<Argument<"a", int>, Argument<"b", float>, Argument<"c", int>>([](int a, float b, int c) {
                                std::cout << "invoked sneed " << a << " " << b << " " << c << std::endl;
                            }).Accepts<Argument<"a", int>>([](int a) {
                                std::cout << "invoked sneed " << a << std::endl;
                            }),
                            Literal<"feed">().Accepts([]{
                                std::cout << "invoked feed" << std::endl;
                            })
                        )
                );
    constexpr auto usage = BuildCommandUsage(tree);
    for (std::string_view str : usage) {
        std::cout << str << std::endl;
    }
    Lexer l{"elytra test sneed 1.2 \"-2.2\" \"1\""};
    auto err = execute_recurse(l, tree);
    if (!err) {
        std::cerr << err.error() << std::endl;
    }
}
