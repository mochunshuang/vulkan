# mingw 不行。需要clang，而clang 和插件整合不太好。也就是无法不依赖idea
set(no_add_test_path "${CMAKE_SOURCE_DIR}/test/no_add_test")

add_executable(no_add_test ${no_add_test_path}/connect8.cpp)
target_compile_features(no_add_test PRIVATE cxx_std_23)