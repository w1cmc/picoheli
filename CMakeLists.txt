# version pi!
cmake_minimum_required(VERSION 3.14)

include(pico_sdk_import.cmake)

set(PROJECT_NAME "picoheli")
project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

include(FetchContent)
FetchContent_Declare(freertos_kernel
	GIT_REPOSITORY "https://github.com/FreeRTOS/FreeRTOS-Kernel.git"
	GIT_TAG "main"
)

add_library(freertos_config INTERFACE)
target_include_directories(
    freertos_config
	INTERFACE
		freertos_config_dir/include
)

target_compile_definitions(
	freertos_config
	INTERFACE
	    configSMP_SPINLOCK_0=PICO_SPINLOCK_ID_OS1
        configSMP_SPINLOCK_1=PICO_SPINLOCK_ID_OS2
)

target_link_libraries(
	freertos_config
	INTERFACE
		hardware_sync
		pico_multicore
)

add_subdirectory(genusb_dir)

# Select the RP2040 FreeRTOS PORT
set(FREERTOS_PORT GCC_RP2040)
# Select the FreeRTOS heap implementation (portable/MemMang/heap_X.c)
set(FREERTOS_HEAP 4)
FetchContent_MakeAvailable(freertos_kernel)
if (NOT freertos_kernel_POPULATED)
	message(SEND_ERROR "freertos_kernel is NOT POPULATED")
endif()

target_link_libraries(freertos_kernel
    PUBLIC
        pico_base
        hardware_sync
        hardware_dma
    PRIVATE
        cmsis_core
        hardware_adc
        hardware_clocks
        hardware_pio
        hardware_rtc
        hardware_spi
        hardware_timer
        pico_multicore
        pico_stdlib
        pico_sync
)

file(GLOB_RECURSE PICOHELI_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
add_executable(picoheli)
target_sources(picoheli PRIVATE ${PICOHELI_SRC_FILES})
target_include_directories(picoheli PRIVATE "include")
target_link_libraries(
    picoheli
    PRIVATE
        cmsis_core
        pico_stdlib
        freertos_kernel
        tinyusb_device
        tinyusb_board
        genusb
        hardware_rtc
        pico_stdlib
)

add_compile_definitions(USE_PRINTF)
add_compile_definitions(PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK)

set_property(TARGET picoheli APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

pico_set_program_name(picoheli "picoheli")
pico_set_program_version(picoheli "0.0")

# target_compile_definitions(picoheli PUBLIC PICO_DEFAULT_UART_TX_PIN=16 PICO_DEFAULT_UART_RX_PIN=17)
pico_enable_stdio_uart(picoheli 0)
pico_enable_stdio_usb(picoheli 1)

pico_add_extra_outputs(picoheli)

message(STATUS "PICO_TINYUSB_PATH: ${PICO_TINYUSB_PATH}")
