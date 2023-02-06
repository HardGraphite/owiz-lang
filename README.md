# The OWIZ programming language

*OWIZ* is a dynamic object-oriented programming language.

> IT IS JUST A TOY!

## How to build

[*CMake*](https://cmake.org/) is used as the build system.
Run the following shell scripts to build *OW* in subdirectory "`build-release`":

```shell
mkdir "build-release"
cd "build-release"
cmake ..
cmake --build .
```

Command line option "`--config Release|Debug|...`"
can be used for "`cmake --build`" command
to specify the build type in some build system like *Visual Studio*.

Modify options `OW_BUILD_SHARED` and `OW_BUILD_STATIC` to specify
whether to build a shared library or to a static library.
It is not recommended to set both of them to `ON` or `OFF`.
By default, `OW_BUILD_SHARED` is `ON` while `OW_BUILD_STATIC` is `OFF`.

Set `OW_TEST` to `ON` to build test files and set up configurations
so that command `ctest` can be used to run tests.
By default, `OW_TEST` is `ON`.

Set `OW_PACK` to `ON` to set up configurations
so that command `cpack` can be used to generate a package.
By default, `OW_PACK` is `OFF`.

By modifying `OW_EMBEDDED_MODULES` and `OW_DYNAMIC_MODULES`,
you can decide which modules are built into the runtime
and which modules are built as standalone files.

Use `ccmake` or `cmake-gui` to view and adjust more options.
