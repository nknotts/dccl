add_executable(dccl_test_logger1 test.cpp)
target_link_libraries(dccl_test_logger1 dccl)

add_test(dccl_test_logger1 ${dccl_BIN_DIR}/dccl_test_logger1)
