include(FetchContent)

if(BUILD_TESTING)
    FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/v3.16.1.tar.gz
        URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
    )
    FetchContent_MakeAvailable(Catch2)
endif()

if(FLOP_BUILD_CONFIG)
    block()
        # yaml-cpp 0.8.0 declares CMake 3.4, which CMake 4 no longer accepts.
        if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.13)
            set(CMAKE_POLICY_VERSION_MINIMUM 3.13)
        endif()
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(YAML_CPP_BUILD_TESTS OFF)
        set(YAML_CPP_BUILD_TOOLS OFF)
        set(YAML_CPP_FORMAT_SOURCE OFF)
        set(YAML_CPP_INSTALL OFF)
        FetchContent_Declare(
            yaml-cpp
            URL https://codeload.github.com/jbeder/yaml-cpp/tar.gz/refs/tags/0.8.0
            DOWNLOAD_NAME yaml-cpp-0.8.0.tar.gz
            URL_HASH SHA256=fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16
        )
        FetchContent_MakeAvailable(yaml-cpp)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # This release relies on an indirect <cstdint> include in emitterutils.cpp.
            set_property(SOURCE "${yaml-cpp_SOURCE_DIR}/src/emitterutils.cpp"
                TARGET_DIRECTORY yaml-cpp APPEND PROPERTY COMPILE_OPTIONS -include cstdint)
        endif()
    endblock()
endif()

