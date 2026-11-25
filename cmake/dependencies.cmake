include(FetchContent)

if(BUILD_TESTING)
    FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/v3.16.1.tar.gz
        URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
    )
    FetchContent_MakeAvailable(Catch2)
endif()

block()
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(YAML_CPP_BUILD_TESTS OFF)
  set(YAML_CPP_BUILD_TOOLS OFF)
  set(YAML_CPP_FORMAT_SOURCE OFF)
  set(YAML_CPP_INSTALL OFF)

  FetchContent_Declare(
      yaml-cpp
      URL https://github.com/jbeder/yaml-cpp/releases/download/yaml-cpp-0.9.0/yaml-cpp-yaml-cpp-0.9.0.tar.gz
      URL_HASH SHA256=298593d9c440fd9034b8b193d96318b76d49bc97c6ceadb7b0836edf0b6d7539
  )
  FetchContent_MakeAvailable(yaml-cpp)
endblock()


if(FLOP_BUILD_GAME)
    # stb_image is dual-licensed under MIT or public domain
    # compile png support only.
    FetchContent_Declare(stb
        URL https://github.com/nothings/stb/archive/f0569113c93ad095470c54bf34a17b36646bbbb5.tar.gz
        URL_HASH SHA256=4d05c96640ae3a8cbdafdad8d344b50ce610802f78aee80154acdb8e266282e0
    )
    FetchContent_MakeAvailable(stb)
    block()
        set(SDL_VIDEO ON)
        set(SDL_GPU ON)
        set(SDL_SHARED ON)
        set(SDL_STATIC OFF)

        set(SDL_TEST_LIBRARY OFF)
        set(SDL_TESTS OFF)
        set(SDL_INSTALL OFF)

        FetchContent_Declare(
            SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
            URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
        )
        FetchContent_MakeAvailable(SDL3)
    endblock()
endif()

