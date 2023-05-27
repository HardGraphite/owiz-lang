#if !OWIZ_EXPORT_API
#    error "`OWIZ_EXPORT_API` is not true"
#endif // !OWIZ_EXPORT_API

#include <owiz.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <compiler/compiler.h>
#include <compiler/error.h>
#include <machine/globals.h>
#include <machine/invoke.h>
#include <machine/machine.h>
#include <machine/modmgr.h>
#include <machine/sysparam.h>
#include <objects/arrayobj.h>
#include <objects/boolobj.h>
#include <objects/cfuncobj.h>
#include <objects/classes.h>
#include <objects/classobj.h>
#include <objects/exceptionobj.h>
#include <objects/floatobj.h>
#include <objects/funcobj.h>
#include <objects/intobj.h>
#include <objects/mapobj.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <objects/setobj.h>
#include <objects/stringobj.h>
#include <objects/symbolobj.h>
#include <objects/tupleobj.h>
#include <utilities/array.h>
#include <utilities/attributes.h>
#include <utilities/platform.h>
#include <utilities/stream.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

#include <config/definitions.h>
#include <config/options.h>
#include <config/version.h>

OWIZ_API union owiz_sysconf_result owiz_sysconf(int name) {
    union owiz_sysconf_result result;

    switch (name) {
    case OWIZ_SC_DEBUG:
        result.i =
#ifdef NDEBUG
            0
#else
            1
#endif
            ;
        break;

    case OWIZ_SC_VERSION:
        result.u = (
            (OW_VERSION_MAJOR << 24) |
            (OW_VERSION_MINOR << 16) |
            (OW_VERSION_PATCH <<  8) );
        static_assert(OW_VERSION_MAJOR <= UINT16_MAX, "");
        static_assert(OW_VERSION_MINOR <= UINT16_MAX, "");
        static_assert(OW_VERSION_PATCH <= UINT16_MAX, "");
        break;

    case OWIZ_SC_VERSION_STR:
        result.s = OW_VERSION_STRING;
        static_assert(sizeof(size_t) >= sizeof(void *), "");
        break;

    case OWIZ_SC_COMPILER:
        result.s = OW_BUILD_COMPILER_NAME " " OW_BUILD_COMPILER_VERSION;
        break;

    case OWIZ_SC_BUILDTIME:
        result.s = __DATE__ " " __TIME__;
        break;

    case OWIZ_SC_PLATFORM:
        result.s = OW_ARCHITECTURE " " OW_SYSTEM_NAME;
        break;

    default:
        memset(&result, 0xff, sizeof result);
        break;
    }

    return result;
}

ow_noinline static int64_t _owiz_sysctl_read_int(const void *val, size_t val_sz) {
    if (val_sz == 8)
        return *(const int64_t *)val;
    if (val_sz == 4)
        return *(const int32_t *)val;
    if (val_sz == 2)
        return *(const int16_t *)val;
    if (val_sz == 1)
        return *(const int8_t *)val;
    return INT64_MIN;
}

OWIZ_API int owiz_sysctl(int name, const void *val, size_t val_sz) {
    switch (name) {
    case OWIZ_CTL_VERBOSE: {
        if (val_sz == (size_t)-1)
            val_sz = strlen(val);
        bool x;
        char w;
        if (*(const char *)val == '!') {
            if (val_sz != 2)
                return OWIZ_ERR_FAIL;
            x = false;
            w = ((const char *)val)[1];
        } else {
            if (val_sz != 1)
                return OWIZ_ERR_FAIL;
            x = true;
            w = *(const char *)val;
        }
        w = (char)toupper(w);
        if (w == 'M')
            ow_sysparam.verbose_memory = x;
        else if (w == 'L')
            ow_sysparam.verbose_lexer = x;
        else if (w == 'P')
            ow_sysparam.verbose_parser = x;
        else if (w == 'C')
            ow_sysparam.verbose_codegen = x;
        else
            return OWIZ_ERR_FAIL;
        return 0;
    }

    case OWIZ_CTL_STACKSIZE: {
        const int64_t v = _owiz_sysctl_read_int(val, val_sz);
        if (v == INT64_MIN)
            return OWIZ_ERR_FAIL;
        return 0;
    }

    case OWIZ_CTL_DEFAULTPATH:
        ow_sysparam_set_string(ow_sysparam_field_offset(default_paths), val, val_sz);
        return 0;

    default:
        return OWIZ_ERR_INDEX;
    }
}

OWIZ_API OWIZ_NODISCARD owiz_machine_t *owiz_create(void) {
    return ow_machine_new();
}

OWIZ_API void owiz_destroy(owiz_machine_t *om) {
    ow_machine_del(om);
}

static_assert(sizeof(struct ow_machine_jmpbuf) <= sizeof(struct owiz_jmpbuf), "");

OWIZ_API void owiz_setjmp(owiz_machine_t *om, owiz_jmpbuf_t env) {
    struct ow_machine_jmpbuf *const jb = (struct ow_machine_jmpbuf *)env;
    ow_machine_setjmp(om, jb);
}

OWIZ_API int owiz_longjmp(owiz_machine_t *om, owiz_jmpbuf_t env) {
    struct ow_machine_jmpbuf *const jb = (struct ow_machine_jmpbuf *)env;
    const bool ok = ow_machine_longjmp(om, jb);
    return ok ? 0 : OWIZ_ERR_FAIL;
}

OWIZ_API void owiz_push_nil(owiz_machine_t *om) {
    *++om->callstack.regs.sp = om->globals->value_nil;
}

OWIZ_API void owiz_push_bool(owiz_machine_t *om, bool val) {
    *++om->callstack.regs.sp =
        val ? om->globals->value_true : om->globals->value_false;
}

OWIZ_API void owiz_push_int(owiz_machine_t *om, intmax_t val) {
    static_assert(sizeof val == sizeof(int64_t), "");
    *++om->callstack.regs.sp = ow_int_obj_or_smallint(om, val);
}

OWIZ_API void owiz_push_float(owiz_machine_t *om, double val) {
    *++om->callstack.regs.sp = ow_object_from(ow_float_obj_new(om, val));
}

OWIZ_API void owiz_push_symbol(owiz_machine_t *om, const char *str, size_t len) {
    assert(str || !len);
    *++om->callstack.regs.sp = ow_object_from(ow_symbol_obj_new(om, str, len));
}

OWIZ_API void owiz_push_string(owiz_machine_t *om, const char *str, size_t len) {
    assert(str || !len);
    *++om->callstack.regs.sp = ow_object_from(ow_string_obj_new(om, str, len));
}

OWIZ_API void owiz_make_array(owiz_machine_t *om, size_t count) {
    struct ow_object **data = om->callstack.regs.sp - count + 1;
    if (ow_unlikely(data < om->callstack.regs.fp)) {
        data = om->callstack.regs.fp;
        count = om->callstack.regs.sp - data + 1;
    }
    *data = ow_object_from(ow_array_obj_new(om, data, count));
    om->callstack.regs.sp = data;
}

OWIZ_API void owiz_make_tuple(owiz_machine_t *om, size_t count) {
    struct ow_object **data = om->callstack.regs.sp - count + 1;
    if (ow_unlikely(data < om->callstack.regs.fp)) {
        data = om->callstack.regs.fp;
        count = om->callstack.regs.sp - data + 1;
    }
    *data = ow_object_from(ow_tuple_obj_new(om, data, count));
    om->callstack.regs.sp = data;
}

OWIZ_API void owiz_make_set(owiz_machine_t *om, size_t count) {
    struct ow_object **data = om->callstack.regs.sp - count + 1;
    if (ow_unlikely(data < om->callstack.regs.fp)) {
        data = om->callstack.regs.fp;
        count = om->callstack.regs.sp - data + 1;
    }
    struct ow_set_obj *const set = ow_set_obj_new(om);
    for (size_t i = 0; i < count; i++)
        ow_set_obj_insert(om, set, data[i]);
    *data = ow_object_from(set);
    om->callstack.regs.sp = data;
}

OWIZ_API void owiz_make_map(owiz_machine_t *om, size_t count) {
    struct ow_object **data = om->callstack.regs.sp - count * 2 + 1;
    if (ow_unlikely(data < om->callstack.regs.fp)) {
        data = om->callstack.regs.fp;
        count = (om->callstack.regs.sp - data + 1) / 2;
    }
    struct ow_map_obj *const map = ow_map_obj_new(om);
    for (size_t i = 0; i < count; i++) {
        struct ow_object **const data_i = data + (i * 2);
        ow_map_obj_set(om, map, data_i[0], data_i[1]);
    }
    *data = ow_object_from(map);
    om->callstack.regs.sp = data;
}

OWIZ_API int owiz_make_exception(owiz_machine_t *om, int type, const char *fmt, ...) {
    ow_unused_var(type); // TODO: Make exception of different type.
    va_list ap;
    va_start(ap, fmt);
    struct ow_exception_obj *const exc_o =
        ow_exception_format(om, NULL, fmt, ap);
    va_end(ap);
    *++om->callstack.regs.sp = ow_object_from(exc_o);
    return 0;
}

/// Compile code from stream and store to the module object on stack top.
/// If error occurs, replace the module object with an exception.
ow_noinline static int _compile_module_from_stream(
    owiz_machine_t *om, const char *file_name, struct ow_stream *source, int flags
) {
    struct ow_compiler *const compiler = ow_compiler_new(om);
    assert(ow_object_class(*om->callstack.regs.sp) == om->builtin_classes->module);
    struct ow_module_obj *const module =
        ow_object_cast(*om->callstack.regs.sp, struct ow_module_obj);
    struct ow_sharedstr *const file_name_ss = ow_sharedstr_new(file_name, (size_t)-1);
    const int compile_flags =
        ow_unlikely(flags & OWIZ_MKMOD_RETLAST) ? OW_COMPILE_RETLASTEXPR : 0;
    const bool ok = ow_compiler_compile(
        compiler, source, file_name_ss, compile_flags, module);
    if (ow_unlikely(!ok)) {
        struct ow_syntax_error *const err = ow_compiler_error(compiler);
        *om->callstack.regs.sp = ow_object_from(ow_exception_format(
            om, NULL, "%s : %u:%u-%u:%u : %s\n", file_name,
            err->location.begin.line, err->location.begin.column,
            err->location.end.line, err->location.end.column,
            ow_sharedstr_data(err->message)));
    }
    ow_sharedstr_unref(file_name_ss);
    ow_compiler_del(compiler);
    return ok ? 0 : OWIZ_ERR_FAIL;
}

OWIZ_API int owiz_make_module(
    owiz_machine_t *om, const char *name, const void *src, int flags
) {
    const int mode = flags & 0xf;

    if (ow_unlikely(flags & OWIZ_MKMOD_INCR)) {
        assert(om->callstack.regs.fp >= om->callstack._data);
        struct ow_object *const v = *om->callstack.regs.sp;
        if (ow_smallint_check(v) ||
                ow_object_class(v) != om->builtin_classes->module)
            *om->callstack.regs.sp = ow_object_from(ow_module_obj_new(om));
    } else {
        struct ow_object *const v = ow_likely(mode != OWIZ_MKMOD_LOAD) ?
            ow_object_from(ow_module_obj_new(om)) : om->globals->value_nil;
        *++om->callstack.regs.sp = v;
    }

    switch (mode) {
    case OWIZ_MKMOD_NATIVE: {
        assert(ow_object_class(*om->callstack.regs.sp) == om->builtin_classes->module);
        struct ow_module_obj *const module =
            ow_object_cast(*om->callstack.regs.sp, struct ow_module_obj);
        const struct ow_native_module_def *const mod_def = src;
        ow_module_obj_load_native_def(om, module, mod_def);
        return 0;
    }

    case OWIZ_MKMOD_FILE: {
        struct ow_stream *const stream =
            ow_stream_open_file(OW_PATH_FROM_STR(src), OW_STREAM_OPEN_READ);
        if (ow_unlikely(!stream)) {
            *om->callstack.regs.sp = ow_object_from(
                ow_exception_format(om, NULL, "cannot open file `%s'", (const char *)src));
            return OWIZ_ERR_FAIL;
        }
        const int status = _compile_module_from_stream(om, src, stream, flags);
        ow_stream_close(stream);
        return status;
    }

    case OWIZ_MKMOD_STRING: {
        struct ow_stream *const stream =
            ow_stream_open_string(src, (size_t)-1, OW_STREAM_OPEN_READ);
        const int status = _compile_module_from_stream(om, "<string>", stream, flags);
        ow_stream_close(stream);
        return status;
    }

    case OWIZ_MKMOD_STDIN:
        return _compile_module_from_stream(om, "<stdin>", ow_stream_stdin(), flags);

    case OWIZ_MKMOD_LOAD: {
        struct ow_exception_obj *exc;
        struct ow_module_obj *const mod =
            ow_module_manager_load(om->module_manager, name, 0, &exc);
        if (ow_unlikely(!mod)) {
            *om->callstack.regs.sp = ow_object_from(exc);
            return OWIZ_ERR_FAIL;
        }
        *om->callstack.regs.sp = ow_object_from(mod);
        return 0;
    }

    default:
        return 0;
    }
}

OWIZ_API int owiz_load_local(owiz_machine_t *om, int index) {
    if (ow_likely(index < 0)) {
        assert(om->callstack.frame_info_list.current);
        struct ow_object **const vp =
            om->callstack.frame_info_list.current->arg_list + (size_t)(-index - 1);
        if (ow_unlikely(vp >= om->callstack.regs.fp))
            return OWIZ_ERR_INDEX;
        *++om->callstack.regs.sp = *vp;
        return 0;
    } else if (ow_likely(index > 0)) {
        assert(om->callstack.regs.fp >= om->callstack._data);
        struct ow_object **const vp = om->callstack.regs.fp + (size_t)(index - 1);
        if (ow_unlikely(vp > om->callstack.regs.sp))
            return OWIZ_ERR_INDEX;
        *++om->callstack.regs.sp = *vp;
        return 0;
    } else {
        return OWIZ_ERR_INDEX;
    }
}

/// Get local variable or argument like `ow_load_local()`. Return top value if index is 0.
/// Return `NULL` if not exists.
static struct ow_object *_get_local(owiz_machine_t *om, int index) {
    if (ow_likely(index < 0)) {
        assert(om->callstack.frame_info_list.current);
        struct ow_object **const vp =
            om->callstack.frame_info_list.current->arg_list + (size_t)(-index - 1);
        if (ow_unlikely(vp >= om->callstack.regs.fp))
            return NULL;
        return *vp;
    } else if (ow_likely(index == 0)) {
        assert(om->callstack.regs.sp >= om->callstack._data);
        return *om->callstack.regs.sp;
    } else {
        assert(om->callstack.regs.sp >= om->callstack.regs.fp);
        struct ow_object **const vp = om->callstack.regs.fp + (size_t)(index - 1);
        if (ow_unlikely(vp > om->callstack.regs.sp))
            return NULL;
        return *vp;
    }
}

OWIZ_API int owiz_load_global(owiz_machine_t *om, const char *name) {
    assert(name);
    assert(om->callstack.frame_info_list.current);
    assert(om->callstack.frame_info_list.current->arg_list - 1 >= om->callstack._data);
    struct ow_object *const func_o =
        om->callstack.frame_info_list.current->arg_list[-1];
    assert(!ow_smallint_check(func_o));
    struct ow_class_obj *const func_class = ow_object_class(func_o);
    struct ow_module_obj *module;
    if (func_class == om->builtin_classes->cfunc)
        module = ow_object_cast(func_o, struct ow_cfunc_obj)->module;
    else if (func_class == om->builtin_classes->func)
        module = ow_object_cast(func_o, struct ow_func_obj)->module;
    else
        return OWIZ_ERR_INDEX;
    struct ow_symbol_obj *const name_o = ow_symbol_obj_new(om, name, (size_t)-1);
    struct ow_object *v = ow_module_obj_get_global_y(module, name_o);
    if (ow_unlikely(!v)) {
        module = om->globals->module_base;
        v = ow_module_obj_get_global_y(module, name_o);
        if (ow_unlikely(!v))
            return OWIZ_ERR_INDEX;
    }
    *++om->callstack.regs.sp = v;
    return 0;
}

OWIZ_API int owiz_load_attribute(owiz_machine_t *om, int index, const char *name) {
    assert(name);
    struct ow_object *const obj = _get_local(om, index);
    if (ow_unlikely(!obj))
        return OWIZ_ERR_INDEX;
    struct ow_class_obj *obj_class;
    if (ow_unlikely(ow_smallint_check(obj)))
        obj_class = om->builtin_classes->int_;
    else
        obj_class = ow_object_class(obj);
    struct ow_symbol_obj *const name_o = ow_symbol_obj_new(om, name, (size_t)-1);
    struct ow_object *attr;
    if (obj_class == om->builtin_classes->module) {
        attr = ow_module_obj_get_global_y(
            ow_object_cast(obj, struct ow_module_obj), name_o);
        if (ow_unlikely(!attr))
            return OWIZ_ERR_INDEX;
    } else {
        const size_t attr_index = ow_class_obj_find_attribute(obj_class, name_o);
        if (ow_likely(attr_index != (size_t)-1)) {
            attr = ow_object_get_field(obj, attr_index);
        } else {
            // TODO: Call __find_attr__() to find attribute.
            return OWIZ_ERR_INDEX;
        }
    }
    *++om->callstack.regs.sp = attr;
    return 0;
}

OWIZ_API void owiz_dup(owiz_machine_t *om, size_t count) {
    assert(om->callstack.regs.sp >= om->callstack.regs.fp);
    struct ow_object *const v = *om->callstack.regs.sp;
    while (count--)
        *++om->callstack.regs.sp = v;
}

OWIZ_API void owiz_swap(owiz_machine_t *om) {
    assert(om->callstack.regs.sp > om->callstack.regs.fp);
    struct ow_object *const tmp = om->callstack.regs.sp[0];
    om->callstack.regs.sp[0] = om->callstack.regs.sp[-1];
    om->callstack.regs.sp[-1] = tmp;
}

OWIZ_API int owiz_read_nil(owiz_machine_t *om, int index) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_likely(ow_class_obj_is_base(
            om->builtin_classes->nil, ow_object_class(v))))
        return 0;
    return OWIZ_ERR_TYPE;
}

OWIZ_API int owiz_read_bool(owiz_machine_t *om, int index, bool *val_p) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v)) {
        return OWIZ_ERR_INDEX;
    } else if (v == om->globals->value_true) {
        *val_p = true;
        return 0;
    } else if (v == om->globals->value_false) {
        *val_p = false;
        return 0;
    } else if (!ow_smallint_check(v) && ow_class_obj_is_base(
            om->builtin_classes->bool_, ow_object_class(v))) {
        *val_p = ow_bool_obj_value(ow_object_cast(v, struct ow_bool_obj));
        return 0;
    } else {
        return OWIZ_ERR_TYPE;
    }
}

OWIZ_API int owiz_read_int(owiz_machine_t *om, int index, intmax_t *val_p) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_likely(ow_smallint_check(v))) {
        *val_p = ow_smallint_from_ptr(v);
        return 0;
    }
    if (ow_likely(ow_class_obj_is_base(
            om->builtin_classes->int_, ow_object_class(v)))) {
        *val_p = ow_int_obj_value(ow_object_cast(v, struct ow_int_obj));
        return 0;
    }
    return OWIZ_ERR_TYPE;
}

OWIZ_API int owiz_read_float(owiz_machine_t *om, int index, double *val_p) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_likely(ow_class_obj_is_base(
            om->builtin_classes->float_, ow_object_class(v)))) {
        *val_p = ow_float_obj_value(ow_object_cast(v, struct ow_float_obj));
        return 0;
    }
    return OWIZ_ERR_TYPE;
}

OWIZ_API int owiz_read_symbol(
    owiz_machine_t *om, int index, const char **str_p, size_t *len_p
) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->symbol, ow_object_class(v))))
        return OWIZ_ERR_TYPE;

    struct ow_symbol_obj *const sym = ow_object_cast(v, struct ow_symbol_obj);
    if (ow_likely(str_p))
        *str_p = ow_symbol_obj_data(sym);
    if (len_p)
        *len_p = ow_symbol_obj_size(sym);
    return 0;
}

OWIZ_API int owiz_read_string(
    owiz_machine_t *om, int index, const char **str_p, size_t *len_p
) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->string, ow_object_class(v))))
        return OWIZ_ERR_TYPE;

    struct ow_string_obj *const str_o = ow_object_cast(v, struct ow_string_obj);
    if (str_p)
        *str_p = ow_string_obj_flatten(om, str_o, len_p);
    else if (len_p)
        *len_p = ow_string_obj_size(str_o);
    return 0;
}

OWIZ_API int owiz_read_string_to(
    owiz_machine_t *om, int index, char *buf, size_t buf_sz
) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->string, ow_object_class(v))))
        return OWIZ_ERR_TYPE;

    struct ow_string_obj *const str_o = ow_object_cast(v, struct ow_string_obj);
    const size_t cp_n = ow_string_obj_copy(str_o, 0, (size_t)-1, buf, buf_sz);
    assert(cp_n <= (size_t)INT_MAX);
    if (cp_n < buf_sz) {
        buf[cp_n] = '\0';
        return (int)cp_n;
    }
    const size_t str_sz = ow_string_obj_size(str_o);
    if (ow_likely(cp_n == str_sz))
        return (int)cp_n;
    assert(cp_n == buf_sz && buf_sz < str_sz);
    return OWIZ_ERR_FAIL;
}

size_t owiz_read_array(owiz_machine_t *om, int index, size_t elem_idx) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return (size_t)OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return (size_t)OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->array, ow_object_class(v))))
        return (size_t)OWIZ_ERR_TYPE;
    struct ow_array *const data =
        ow_array_obj_data(ow_object_cast(v, struct ow_array_obj));
    const size_t len = ow_array_size(data);
    assert(len < SIZE_MAX / 2);
    if (ow_likely(elem_idx >= 1 && elem_idx <= len)) {
        *++om->callstack.regs.sp = ow_array_at(data, elem_idx - 1);
        return 0;
    }
    if (elem_idx == 0)
        return len;
    if (elem_idx == (size_t)-1) {
        for (size_t i = 0; i < len; i++)
            *++om->callstack.regs.sp = ow_array_at(data, i);
        return len;
    }
    return (size_t)OWIZ_ERR_FAIL;
}

size_t owiz_read_tuple(owiz_machine_t *om, int index, size_t elem_idx) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return (size_t)OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return (size_t)OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->tuple, ow_object_class(v))))
        return (size_t)OWIZ_ERR_TYPE;
    struct ow_tuple_obj *const tuple_obj = ow_object_cast(v, struct ow_tuple_obj);
    struct ow_object *const elem = ow_tuple_obj_get(tuple_obj, elem_idx - 1);
    if (ow_likely(elem)) {
        *++om->callstack.regs.sp = elem;
        return 0;
    }
    const size_t len = ow_tuple_obj_length(tuple_obj);
    assert(len < SIZE_MAX / 2);
    if (elem_idx == 0)
        return len;
    if (elem_idx == (size_t)-1) {
        struct ow_object **const start = om->callstack.regs.sp + 1;
        om->callstack.regs.sp += len;
        const size_t copied_cnt = ow_tuple_obj_copy(tuple_obj, 0, len, start, len);
        ow_unused_var(copied_cnt);
        assert(copied_cnt == len);
        return len;
    }
    return (size_t)OWIZ_ERR_FAIL;
}

static int _set_expand_walker(void *arg, struct ow_object *elem) {
    struct ow_machine *const om = arg;
    *++om->callstack.regs.sp = elem;
    return 0;
}

OWIZ_API size_t owiz_read_set(owiz_machine_t *om, int index, int op) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return (size_t)OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return (size_t)OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->set, ow_object_class(v))))
        return (size_t)OWIZ_ERR_TYPE;
    struct ow_set_obj *const set_obj = ow_object_cast(v, struct ow_set_obj);
    const size_t len = ow_set_obj_length(set_obj);
    assert(len < SIZE_MAX / 2);
    if (op == 0)
        return len;
    if (op == -1) {
        ow_set_obj_foreach(set_obj, _set_expand_walker, om);
        return len;
    }
    return (size_t)OWIZ_ERR_ARG;
}

static int _map_expand_walker(
    void *arg, struct ow_object *key, struct ow_object *val
) {
    struct ow_machine *const om = arg;
    *++om->callstack.regs.sp = key;
    *++om->callstack.regs.sp = val;
    return 0;
}

OWIZ_API size_t owiz_read_map(owiz_machine_t *om, int index, int key_idx) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return (size_t)OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return (size_t)OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->map, ow_object_class(v))))
        return (size_t)OWIZ_ERR_TYPE;
    struct ow_map_obj *const map_obj = ow_object_cast(v, struct ow_map_obj);
    if (key_idx >= -255) {
        struct ow_object *const kv = _get_local(om, key_idx);
        if (!kv)
            return (size_t)OWIZ_ERR_FAIL;
        struct ow_object *const res = ow_map_obj_get(om, map_obj, kv);
        if (ow_unlikely(!res))
            return (size_t)OWIZ_ERR_FAIL;
        *++om->callstack.regs.sp = res;
        return 0;
    }
    const size_t len = ow_map_obj_length(map_obj);
    if (key_idx == OWIZ_RDMAP_GETLEN)
        return len;
    if (key_idx == OWIZ_RDMAP_EXPAND) {
        ow_map_obj_foreach(map_obj, _map_expand_walker, om);
        return len;
    }
    return (size_t)OWIZ_ERR_FAIL;
}

OWIZ_API int owiz_read_exception(owiz_machine_t *om, int index, int flags, ...) {
    struct ow_object *const v = _get_local(om, index);
    if (ow_unlikely(!v))
        return OWIZ_ERR_INDEX;
    if (ow_unlikely(ow_smallint_check(v)))
        return OWIZ_ERR_TYPE;
    if (ow_unlikely(!ow_class_obj_is_base(
            om->builtin_classes->exception, ow_object_class(v))))
        return OWIZ_ERR_TYPE;

    const int flags_target = flags & 0xf0;
    struct ow_exception_obj *const exc_o =
        ow_object_cast(v, struct ow_exception_obj);
    if (flags_target == OWIZ_RDEXC_PUSH) {
        *++om->callstack.regs.sp = ow_exception_obj_data(exc_o);
    } else {
        va_list ap;
        struct ow_stream *out_stream;
        va_start(ap, flags);
        if (flags_target == OWIZ_RDEXC_PRINT) {
            FILE *const fp = va_arg(ap, FILE *);
            out_stream = fp == stdout ? ow_stream_stdout() : ow_stream_stderr();
        } else if (flags_target == OWIZ_RDEXC_TOBUF) {
            out_stream = ow_stream_open_string(NULL, 0, OW_STREAM_OPEN_WRITE);
        } else {
            out_stream = NULL;
        }
        int pri_flags = 0;
        if (flags & OWIZ_RDEXC_MSG)
            pri_flags |= 1;
        if (flags & OWIZ_RDEXC_BT)
            pri_flags |= 2;
        ow_exception_obj_print(om, exc_o, out_stream, pri_flags);
        if (flags_target == OWIZ_RDEXC_TOBUF) {
            size_t data_size;
            const char *const data =
                ow_string_stream_data((struct ow_string_stream *)out_stream, &data_size);
            char *const buf = va_arg(ap, char *);
            const size_t buf_size = va_arg(ap, size_t);
            memcpy(buf, data, buf_size > data_size ? data_size : buf_size);
            buf[buf_size > data_size ? data_size : buf_size - 1] = '\0';
            ow_stream_close(out_stream);
        }
        va_end(ap);
    }

    return 0;
}

OWIZ_API int owiz_read_args(owiz_machine_t *om, int flags, const char *fmt, ...) {
    va_list ap;
    int status;

    if (ow_unlikely(!fmt)) {
        const unsigned int argc =
            (unsigned int)(om->callstack.regs.fp - 1
                - om->callstack.frame_info_list.current->arg_list);
        va_start(ap, fmt);
        *va_arg(ap, unsigned int *) = argc;
        va_end(ap);
        return 0;
    }

    va_start(ap, fmt);
    status = 0;
    for (int index = -1; ; index--) {
        const char specifier = *fmt++;
        if (ow_unlikely(!specifier))
            break;

        switch (specifier) {
        case 'x':
            status = owiz_read_bool(om, index, va_arg(ap, bool *));
            break;
        case 'i':
            status = owiz_read_int(om, index, va_arg(ap, intmax_t *));
            break;
        case 'f':
            status = owiz_read_float(om, index, va_arg(ap, double *));
            break;
        case 'y': {
            const char **const s = va_arg(ap, const char **);
            size_t *const n = va_arg(ap, size_t *);
            status = owiz_read_symbol(om, index, s, n);
        }
            break;
        case 's':
            if (*fmt == '*') {
                fmt++;
                char *const s = va_arg(ap, char *);
                const size_t n = va_arg(ap, size_t);
                status = owiz_read_string_to(om, index, s, n);
            } else {
                const char **const s = va_arg(ap, const char **);
                size_t *const n = va_arg(ap, size_t *);
                status = owiz_read_string(om, index, s, n);
            }
            break;
        default:
            status = OWIZ_ERR_ARG;
            break;
        }

        if (ow_unlikely(status != 0)) {
            if (status == OWIZ_ERR_TYPE) {
                if (flags & OWIZ_RDARG_IGNIL && owiz_read_nil(om, index) == 0) {
                    status = 0;
                    continue;
                }
                if (flags & OWIZ_RDARG_MKEXC) {
                    const char *const type_name =
                        ow_symbol_obj_data(ow_class_obj_pub_info(
                            ow_object_class(_get_local(om, index)))->class_name);
                    *++om->callstack.regs.sp = ow_object_from(ow_exception_format(om, NULL,
                        "unexpected %s object for argument %i", type_name, -index));
                    status = OWIZ_ERR_FAIL;
                }
                break;
            } else if (status == OWIZ_ERR_FAIL) {
                if (flags & OWIZ_RDARG_MKEXC) {
                    *++om->callstack.regs.sp = ow_object_from(
                        ow_exception_format(om, NULL, "illegal usage of API"));
                    status = OWIZ_ERR_FAIL;
                }
                break;
            } else {
                break;
            }
        }
    }
    va_end(ap);

    return status;
}

OWIZ_API int owiz_store_local(owiz_machine_t *om, int index) {
    if (ow_likely(index < 0)) {
        assert(om->callstack.frame_info_list.current);
        struct ow_object **const vp =
            om->callstack.frame_info_list.current->arg_list + (size_t)(-index);
        if (ow_unlikely(vp >= om->callstack.regs.fp))
            return OWIZ_ERR_INDEX;
        *vp = *om->callstack.regs.sp--;
        return 0;
    } else if (ow_likely(index > 0)) {
        assert(om->callstack.regs.fp >= om->callstack._data);
        struct ow_object **const vp = om->callstack.regs.fp + (size_t)(index - 1);
        if (ow_unlikely(vp > om->callstack.regs.sp))
            return OWIZ_ERR_INDEX;
        *vp = *om->callstack.regs.sp--;
        return 0;
    } else {
        return OWIZ_ERR_INDEX;
    }
}

OWIZ_API int owiz_store_global(owiz_machine_t *om, const char *name) {
    assert(name);
    assert(om->callstack.frame_info_list.current);
    assert(om->callstack.frame_info_list.current->arg_list - 1 >= om->callstack._data);
    struct ow_object *const func_o =
        om->callstack.frame_info_list.current->arg_list[-1];
    assert(!ow_smallint_check(func_o));
    struct ow_class_obj *const func_class = ow_object_class(func_o);
    struct ow_module_obj *module;
    if (func_class == om->builtin_classes->cfunc)
        module = ow_object_cast(func_o, struct ow_cfunc_obj)->module;
    else if (func_class == om->builtin_classes->func)
        module = ow_object_cast(func_o, struct ow_func_obj)->module;
    else
        return OWIZ_ERR_INDEX;
    struct ow_symbol_obj *const name_o = ow_symbol_obj_new(om, name, (size_t)-1);
    ow_module_obj_set_global_y(module, name_o, *om->callstack.regs.sp--);
    return 0;
}

OWIZ_API int owiz_drop(owiz_machine_t *om, int count) {
    struct ow_object **const fp_m1 = om->callstack.regs.fp - 1;
    struct ow_object **new_sp = om->callstack.regs.sp - (size_t)(unsigned)count;
    if (new_sp < fp_m1)
        new_sp = fp_m1;
    om->callstack.regs.sp = new_sp;
    return (int)(new_sp - fp_m1);
}

OWIZ_API int owiz_invoke(owiz_machine_t *om, int argc, int flags) {
    int status;
    struct ow_object *result;

    assert(argc >= 0);
    assert(om->callstack.regs.sp >= om->callstack.regs.fp + argc);

    const int mode = flags & 0xf;
    if (mode == 0) {
        status = ow_machine_invoke(om, argc, &result);
    } else if (mode == OWIZ_IVK_METHOD) {
        struct ow_object *const name_o = *(om->callstack.regs.sp - argc);
        if (ow_unlikely(ow_smallint_check(name_o) ||
                ow_object_class(name_o) != om->builtin_classes->symbol)) {
            result = ow_object_from(ow_exception_format(
                om, NULL, "%s is not a %s object", "method name", "Symbol"));
            status = OWIZ_ERR_FAIL;
            goto end_invoke;
        }
        status = ow_machine_call_method(
            om, ow_object_cast(name_o, struct ow_symbol_obj), argc, NULL, &result);
    } else if (mode == OWIZ_IVK_MODULE) {
        struct ow_object *const mod_o = *(om->callstack.regs.sp - argc);
        if (ow_unlikely(ow_smallint_check(mod_o) ||
                ow_object_class(mod_o) != om->builtin_classes->module)) {
            result = ow_object_from(ow_exception_format(
                om, NULL, "%s is not a %s object", "module", "Module"));
            status = OWIZ_ERR_FAIL;
            goto end_invoke;
        }
        const bool call_main = flags & OWIZ_IVK_MODMAIN;
        status = ow_machine_run(
            om, ow_object_cast(mod_o, struct ow_module_obj), call_main, &result);
    } else {
        ow_unreachable();
    }

end_invoke:
    if (ow_unlikely(status) || !(flags & OWIZ_IVK_NORETVAL))
        *++om->callstack.regs.sp = result;

    assert(status == 0 || status == OWIZ_ERR_FAIL);
    return status;
}

OWIZ_API int owiz_syscmd(owiz_machine_t *om, int name, ...) {
    int status = 0;
    va_list ap;
    va_start(ap, name);

    switch (name) {
    case OWIZ_CMD_ADDPATH:
        if (!ow_module_manager_add_path(om->module_manager, va_arg(ap, const char *)))
            status = OWIZ_ERR_FAIL;
        break;

    default:
        status = OWIZ_ERR_INDEX;
        break;
    }

    va_end(ap);
    return status;
}
