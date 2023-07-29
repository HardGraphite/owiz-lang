#include "sysparam.h"

#include <assert.h>
#include <string.h>

#include <utilities/memalloc.h>

volatile struct ow_sysparam ow_sysparam = {
    .stack_size       = 4000 / sizeof(void *),
    .default_paths    = NULL,
};

void ow_sysparam_set_string(size_t off, const char *str, size_t len) {
    assert(off >= ow_sysparam_field_offset(default_paths));
    assert(off < sizeof(struct ow_sysparam) - sizeof(char *));
    assert(len != (size_t)-1);

    char *const s = ow_malloc(len + 1);
    memcpy(s, str, len);
    s[len] = '\0';

    // TODO: Use atomic operation or mutex.
    char **const val_ptr = (char **)((unsigned char *)&ow_sysparam + off);
    char *const old_s = *val_ptr;
    *val_ptr = s;
    if (old_s)
        ow_free(old_s);
}
