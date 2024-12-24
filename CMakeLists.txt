# version pi!
cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_DEOPTIMIZED_DEBUG ON CACHE BOOL "Build for debug" FORCE)
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picoheli C CXX ASM)

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

set(TINYUSB_OPT_OS OPT_OS_FREERTOS)
pico_sdk_init()

# Select the RP2040 FreeRTOS PORT
set(FREERTOS_PORT GCC_RP2040)
# Select the FreeRTOS heap implementation (portable/MemMang/heap_X.c)
set(FREERTOS_HEAP 4)
FetchContent_MakeAvailable(freertos_kernel)
if (NOT freertos_kernel_POPULATED)
	message(SEND_ERROR "freertos_kernel is NOT POPULATED")
endif()

target_compile_definitions(tinyusb_common INTERFACE CFG_TUSB_OS_INC_PATH=${freertos_kernel_SOURCE_DIR}/include/)

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
        hardware_rtc
        hardware_pio
        hardware_dma
        pico_stdlib
        pico_unique_id
)

string(TIMESTAMP BUILD_INFO "%b %d %Y%H:%M:%S")
target_compile_definitions(picoheli PRIVATE BUILD_INFO="${BUILD_INFO}")

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
