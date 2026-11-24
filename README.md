# flop

An ongoing C++ port of [asherhe/unity-orbit](https://github.com/asherhe/unity-orbit).

## Dependencies

- CMake 3.31.8 or newer
- GCC (`g++`)
- Ninja
- Catch2 3.16.1 (fetched by CMake for tests)

## Build and test

Configure the test build with the `tests` preset. This selects Ninja and enables tests:

```sh
cmake --preset tests
```

Build the library and both test executables:

```sh
cmake --build --preset tests
```

Run the tests:

```sh
ctest --preset tests
```

Alternatively, configure, build, and run the tests in one step with the workflow preset:

```sh
cmake --workflow --preset tests
```

The test preset builds and runs both core and application tests without opening a
window. The current tests do not require SDL, a GPU, or shader tools; some
application tests load source assets. A headless test may still use SDL without
opening a window.
To build and run just one group after configuring, use its matching presets:

```sh
cmake --build --preset test-core
ctest --preset test-core
cmake --build --preset test-app
ctest --preset test-app
```

Build presets select targets; CTest presets select registered tests by label. Build
the matching target before running a filtered test preset. The labels are `core`
and `app`; the `tests` test preset runs both groups.
