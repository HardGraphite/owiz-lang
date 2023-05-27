#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Global parameters.
struct ow_sysparam {
    bool verbose_memory;
    bool verbose_lexer;
    bool verbose_parser;
    bool verbose_codegen;
    size_t stack_size; // Number of objects.
    char *default_paths; // Default module paths.
};

/// Global parameters.
extern volatile struct ow_sysparam ow_sysparam;

/// Assign to a string value in `ow_sysparam`.
void ow_sysparam_set_string(size_t off, const char *str, size_t len);

#define ow_sysparam_field_offset(FIELD) offsetof(struct ow_sysparam, FIELD)
