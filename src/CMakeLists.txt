add_library(flop_core STATIC
    flop/app/game_scene.cpp
    flop/app/tick_scheduler.cpp
    flop/craft/craft.cpp
    flop/craft/newtonian.cpp
    flop/craft/resources.cpp
    flop/craft/resource_registry.cpp
    flop/craft/parts/part.cpp
    flop/craft/plugins/plugin_factory.cpp
    flop/craft/plugins/engine.cpp
    flop/craft/plugins/resource.cpp
    flop/config/asset_path.cpp
    flop/config/load.cpp
    flop/orbit/ref_frame.cpp
    flop/orbit/state.cpp
    flop/orbit/prediction/encounter.cpp
    flop/orbit/prediction/patched_conics.cpp
    flop/orbit/prediction/soi_escape.cpp
    flop/orbit/prediction/soi_intercept.cpp
    flop/orbit/propagators/universal.cpp
    flop/view/frame.cpp
    flop/view/lighting.cpp
    flop/view/particle_scene.cpp
    flop/view/particles.cpp
    flop/world/control_manager.cpp
    flop/world/celestial_body.cpp
    flop/world/universe.cpp
)
target_include_directories(flop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(flop_core PUBLIC cxx_std_20)
target_link_libraries(flop_core PUBLIC yaml-cpp::yaml-cpp)

if(FLOP_BUILD_GAME)
    add_executable(flop
        flop/render/material_recipe.cpp
        flop/app/app.cpp
        flop/app/main.cpp
        flop/input/pilot_control.cpp
        flop/platform/input_mapper.cpp
        flop/platform/sdl_platform.cpp
        flop/render/renderer.cpp
        flop/render/texture_cache.cpp
    )

    set_target_properties(flop PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${FLOP_DIST_DIR}"
    )

    target_link_libraries(flop PRIVATE flop_core SDL3::SDL3 flop_shader_assets)
    target_include_directories(flop SYSTEM PRIVATE "${stb_SOURCE_DIR}")

    # copy assets alongside executable
    add_custom_target(flop_assets
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${FLOP_ASSETS_DIR}"
            "$<TARGET_FILE_DIR:flop>/assets"
        COMMENT "Copying runtime assets"
        VERBATIM
    )
    add_dependencies(flop flop_assets)

    # package SDL3.dll alongside flop.exe on Windows.
    if(WIN32)
        add_custom_target(flop_sdl_runtime
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "$<TARGET_FILE_DIR:flop>"
            DEPENDS SDL3::SDL3-shared
            COMMENT "Copying SDL runtime DLL"
            VERBATIM
        )
        add_dependencies(flop flop_sdl_runtime)
    endif()

    # copy runtime .dlls on windows if necessary
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(flop_gcc_runtime_dlls)
        get_filename_component(flop_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            unset(dll_path CACHE)
            find_file(dll_path NAMES "${dll}"
                HINTS "${flop_compiler_dir}" ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}
                NO_DEFAULT_PATH
            )
            if(NOT dll_path)
                message(FATAL_ERROR "Could not locate MinGW runtime ${dll} near ${CMAKE_CXX_COMPILER}")
            endif()
            list(APPEND flop_gcc_runtime_dlls "${dll_path}")
        endforeach()
        add_custom_target(flop_gcc_runtime
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${flop_gcc_runtime_dlls} "$<TARGET_FILE_DIR:flop>"
            COMMENT "Copying MinGW runtime DLLs"
            VERBATIM
        )
        add_dependencies(flop flop_gcc_runtime)
    endif()
endif()

