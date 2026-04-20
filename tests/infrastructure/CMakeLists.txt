cmake_minimum_required(VERSION 3.14)

# Logger test
add_executable(logger_test logger_test.cpp)
target_link_libraries(logger_test PRIVATE avsdk_infrastructure GTest::gtest_main)
add_test(NAME logger_test COMMAND logger_test)

# MemoryPool test
add_executable(memory_pool_test memory_pool_test.cpp)
target_link_libraries(memory_pool_test PRIVATE avsdk_infrastructure GTest::gtest_main)
add_test(NAME memory_pool_test COMMAND memory_pool_test)
