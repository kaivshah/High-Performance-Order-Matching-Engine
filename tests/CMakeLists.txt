add_executable(test_order_book test_order_book.cpp)

target_link_libraries(test_order_book
    PRIVATE
    engine_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_order_book)