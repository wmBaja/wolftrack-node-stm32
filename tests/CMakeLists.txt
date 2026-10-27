cmake_minimum_required(VERSION 3.22)

project(wolftrack_host_tests LANGUAGES CXX)

enable_testing()

add_executable(app_logic_tests
    test_app_logic.cpp
    ../App/Src/app_logic.cpp
)

target_include_directories(app_logic_tests PRIVATE ../App/Inc)
target_compile_features(app_logic_tests PRIVATE cxx_std_17)
target_compile_options(app_logic_tests PRIVATE
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
)

add_test(NAME app_logic_tests COMMAND app_logic_tests)
