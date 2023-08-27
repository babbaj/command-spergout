cmake_minimum_required(VERSION 3.26)
project(commandcpp)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libhat)

add_executable(commandcpp main.cpp ArgumentType.hpp CommandArguments.hpp CommandNode.hpp DefaultArgumentTypes.hpp ArgumentType.hpp
        Command.h CommandExecutor.h Types.h)
