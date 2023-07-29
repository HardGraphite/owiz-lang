# Coding Style and Conventions

## Common

- Use spaces for indentation (there are exceptions like Makefile).
- Use UTF-8 encoding. Do not use non-ASCII characters if possible.
- Use LF (`U+000A`) as line separators.
- No trailing whitespace. File ends with a new line.

## C/C++

Coding style is very personal.
No tool is going to be used for code formatting.
Most rules here are recommendations
and can be broken conditionally for readability.

### Indentation and line width

- Use spaces instead of tabs for indentation.
- Each level of indentation must be 4 spaces.
- The recommended maximum width for a line is 80 columns. Never exceed 100.
- Indentations for preprocessing directives are inserted after "`#`".

Here is an example:

```c
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#    include <unistd.h>
#elif defined(_WIN32)
#    include <io.h>
#    define write(fd, buf, sz) _write(fd, buf, sz)
#else
#    error Oops!
#endif

void write_str(int fd, const char *s) {
    if (s) {
        const size_t n = strlen(s);
        write(fd, s, n);
    }
}
```

### Braces and spaces

For a code block,
Opening brace shall be put at the end of a line
and closing brace shall be put at the beginning of a line.
Unnecessary braces like that around a single statement shall not appear.

Here are examples:

```c
if (condition) {
    foo();
    bar();
}

if (condition)
    baz();
```

- Use a space after keywords `if`, `switch`, `case`, `for`, `while`, `do`.
- Use a space around (on each side of) binary and ternary operators.
- Use a space around (on each side of) "`//`" and "`/*`"
unless they are at the beginning of a line.
- May add extra spaces around operators to align them.
- No space around the `.` and `->` operators.
- No space after unary operators.
- No trailing whitespace.

Here are examples:

```c
span_size  = span->size * sizeof span->data[0];
total_size = span_size * span_count;
```

### Breaking long lines

A line longer than 80 columns shall be broken,
unless it increases readability to keep them in a line.
Prefer block indent rather than visual indent.
Indentation level shall be increased for wrapped line,
but do not insert extra spaces to align with the first line.
To break parentheses in function argument list
or `if () {}`, `for () {}`, `while () {}` statements,
start new line (optional) after opening parenthesis ("`(`")
and move the closing parenthesis and opening brace ("`) {`") to a new line
without extra indentation level.
If return type and specifiers are too long (longer than 40 columns)
while function name and argument list are short,
instead of breaking the argument list,
the return type and specifiers can be put in stand-alone lines.

Here is an example:

```c
void hello(const char *who_v[], size_t who_n, FILE *stream);

__attribute__((always_inline)) inline static int hi(const char *who) {
    hello(&who, 1, stdout);
    return 0;
}

void hello(const char *who_v[], size_t who_n, FILE *stream) {
    for (size_t i = 0; i < who_n; i++)
        fprintf(stream, "Hello, %s!\n", who_v[i]);
}

// vvvvvvvvvvvvvvvvvvvvvvvv //
void hello(const char *who_v[],
    size_t who_n, FILE *stream); // <==

__attribute__((always_inline))
inline static int              // <==
hi(const char *who) {
    hello(&who, 1, stdout);
    return 0;
}

void hello(                    // <==
    const char *who_v[],
    size_t who_n,
    FILE *stream
) {                            // <==
    for (size_t i = 0;
        i < who_n; i++
    ) {
        const char *who =
            who_v[i];          // <==
        fprintf(               // <==
            stream,
            "Hello, %s!\n",
            who
        );                     // <==
    }
}
```

### Naming

- Identifiers (namespaces, variables, functions, and types) shall be `snake_case`.
- Macros shall be `SCREAMING_SNAKE_CASE`.
- Use "`ow`" (private API) or "`owiz`" (public API) as identifier prefix (C) or namespace (C++).

## CMake

- Use similar styles with C/C++.
- No spaces after `if`, `foreach`, and `while`.

## Python

Follow the [PEP 8](https://peps.python.org/pep-0008/) guide.
