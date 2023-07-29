# The OWIZ Programming Language

*OWIZ* is a dynamic object-oriented programming language.

> IT IS JUST A TOY!

## Building

[*CMake*](https://cmake.org/) is used as the build system.
Run the following shell scripts to build *OWIZ* in subdirectory "`build-dir`":

```shell
mkdir "build-dir"
cd "build-dir"
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

## Developing

Set CMake option `OW_DEVELOPING` to `ON` for development.
Read [coding style](doc/coding_style.md) for style guide.

Set CMake option `OW_DEBUGLOG` to `ON` to enable debug logging.
By default, the minimum log level to print and output file can be controlled
by an environment variable called `OWIZ_DEBUGLOG`.
See CMake variables `OW_DEBUG_LOGGING_DEFAULT` and `OW_DEBUG_LOGGING_ENVNAME`
for further customizations about debug logging.

## Code File Organization

| Directory | Description                                      |
|-----------|--------------------------------------------------|
| `cmake`   | CMake utilities.                                 |
| `doc`     | Design documentations.                           |
| `include` | C/C++ header files.                              |
| `src`     | C source files of the runtime.                   |
| `test`    | Test code.                                       |
| `tool`    | Executable tools.                                |

| Directory       | Description                                        |
|-----------------|----------------------------------------------------|
| `src/bytecode`  | Byte code definitions and utilities.               |
| `src/compat`    | Compatibility code.                                |
| `src/compiler`  | Byte code compiler.                                |
| `src/machine`   | The virtual machine.                               |
| `src/modules`   | Native modules. Configurable.                      |
| `src/objects`   | Object basics and built-in types.                  |
| `src/toplevels` | Entry points to executables and libraries.         |
| `src/utilities` | Runtime-independent utilities.                     |
