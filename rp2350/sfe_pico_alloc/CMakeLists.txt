add_library(sfe_pico_alloc INTERFACE)

target_sources(
    sfe_pico_alloc
    INTERFACE ${CMAKE_CURRENT_LIST_DIR}/sfe_pico_alloc.c
              ${CMAKE_CURRENT_LIST_DIR}/sfe_pico_alloc.h
              ${CMAKE_CURRENT_LIST_DIR}/../lib/sfe_pico_boards.h
              ${CMAKE_CURRENT_LIST_DIR}/../lib/sfe_psram.h
              ${CMAKE_CURRENT_LIST_DIR}/../lib/sfe_psram.c
              ${CMAKE_CURRENT_LIST_DIR}/tlsf/tlsf.c
              ${CMAKE_CURRENT_LIST_DIR}/tlsf/tlsf.h
              ${CMAKE_CURRENT_LIST_DIR}/tlsf/tlsf_common.h
              ${CMAKE_CURRENT_LIST_DIR}/tlsf/tlsf_block_functions.h)

# Are we wrapping the built in alloc functions?
if (SFE_PICO_ALLOC_WRAP)
    pico_wrap_function(sfe_pico_alloc malloc)
    pico_wrap_function(sfe_pico_alloc calloc)
    pico_wrap_function(sfe_pico_alloc realloc)
    pico_wrap_function(sfe_pico_alloc free)
endif ()

target_link_libraries(sfe_pico_alloc INTERFACE pico_stdlib hardware_spi hardware_gpio pico_flash
                                               hardware_exception hardware_sync)
