# version pi!
cmake_minimum_required(VERSION 3.14)

if (DEFINED ENV{PICO_SDK_PATH})
    unset(ENV{PICO_SDK_PATH})
endif()
set(PICO_SDK_FETCH_FROM_GIT ON CACHE BOOL "Fetch the Pico SDK from github")
set(PICO_SDK_FETCH_FROM_GIT_TAG "2.1.0" CACHE STRING "Git tag to check out when fetching SDK")
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
        hardware_pio
        hardware_dma
        pico_stdlib
)

add_compile_definitions(USE_PRINTF)
pico_enable_stdio_uart(picoheli 1)
if($<BOOL:LIB_PICO_STDIO_UART>)
    add_compile_definitions(PICO_STDIO_UART_SUPPORT_CHARS_AVAILABLE_CALLBACK=1)
else()
    add_compile_definitions(PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK=1)
endif()
# target_compile_definitions(picoheli PUBLIC PICO_DEFAULT_UART_TX_PIN=16 PICO_DEFAULT_UART_RX_PIN=17)
# always turn off stdio over USB; we handle that ourselves in TinyUSB.
pico_enable_stdio_usb(picoheli 0)

set_property(TARGET picoheli APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

pico_set_program_name(picoheli "picoheli")
pico_set_program_version(picoheli "0.0")
pico_add_extra_outputs(picoheli)

pico_generate_pio_header(picoheli ${CMAKE_CURRENT_LIST_DIR}/src/onewire.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
target_include_directories(picoheli PUBLIC ${CMAKE_CURRENT_LIST_DIR}/generated)
