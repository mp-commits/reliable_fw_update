project(common)

add_library(${PROJECT_NAME}
    STATIC
        driver_w25qxx.c
        no_init_ram.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
)

add_library(libs::common ALIAS ${PROJECT_NAME})
