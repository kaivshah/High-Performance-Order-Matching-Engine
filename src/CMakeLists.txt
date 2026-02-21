# Create a library for the core engine logic to be shared with tests
add_library(core_engine
    order.h
    order_book.h
    order_book.cpp
    engine.h
    engine.cpp
)

# Main executable
add_executable(OrderMatchingEngine main.cpp)
target_link_libraries(OrderMatchingEngine PRIVATE core_engine)
