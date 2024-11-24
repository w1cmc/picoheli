add_library(genusb STATIC tusb_port.c)
target_include_directories(genusb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    genusb
    PRIVATE
        tinyusb_board
        tinyusb_device
        pico_unique_id
        pico_binary_info
        pico_stdlib
)
