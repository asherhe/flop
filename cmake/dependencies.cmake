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

