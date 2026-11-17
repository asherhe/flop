# flop

An ongoing C++ port of [asherhe/unity-orbit](https://github.com/asherhe/unity-orbit).

## Dependencies

- CMake 3.31.8 or newer
- GCC (`g++`)
- Ninja
- Catch2 3.16.1 (fetched by CMake for tests)

## Build and test

Configure the headless build with the `headless` preset. This selects Ninja, GCC, and enables tests:

```sh
cmake --preset headless
```

Build the library and test executable:

```sh
cmake --build --preset headless
```

Run the tests:

```sh
ctest --preset headless
```

Alternatively, configure, build, and run the tests in one step with the workflow preset:

```sh
cmake --workflow --preset headless
```
