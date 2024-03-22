cmake_minimum_required(VERSION 3.20)

project(ExecutionChain)

find_package(gtest CONFIG REQUIRED)

add_library(execution_chain INTERFACE
    execution_chain/ExecutionChain.h
    execution_chain/ExecutionFlow.h
    execution_chain/polymorphic_value.h
)

set_property(TARGET execution_chain PROPERTY CXX_STANDARD 20)

add_executable(tests
    tests/TestExecutionChain.cpp
    tests/main.cpp
)

target_link_libraries(tests PRIVATE GTest::gtest_main)

set_property(TARGET tests PROPERTY CXX_STANDARD 20)
