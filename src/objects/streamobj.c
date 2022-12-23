#include "streamobj.h"

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include "symbolobj.h"
#include <machine/globals.h>
#include <machine/invoke.h>
#include <machine/machine.h>

struct stream_obj_stream {
	OW_STREAM_HEAD
	struct ow_machine *machine;
	struct ow_stream_obj *object;
};

static const struct ow_stream_operations sos_operations;

static void sos_gc_marker(struct ow_machine *om, void *_sos) {
	struct stream_obj_stream *const stream = _sos;
	assert(stream->machine == om);
	ow_objmem_object_gc_marker(om, ow_object_from(stream));
}

static struct stream_obj_stream *sos_init(
		struct stream_obj_stream *stream, struct ow_machine *om, struct ow_stream_obj *obj) {
	stream->_ops = &sos_operations;
	stream->_open_flags = OW_STREAM_OPEN_READ | OW_STREAM_OPEN_WRITE;
	stream->machine = om;
	stream->object = obj;
	ow_objmem_add_gc_root(om, stream, sos_gc_marker);
	return stream;
}

static void sos_fini(struct stream_obj_stream *stream) {
	ow_objmem_remove_gc_root(stream->machine, stream);
}

static bool sos_eof(const struct stream_obj_stream *stream) {
	struct ow_object *res;
	const int status = ow_machine_call_method(
		stream->machine,
		ow_symbol_obj_new(stream->machine, "eof", 3),
		1, (struct ow_object *[]){
			ow_object_from(stream->object)
		},
		&res
	);
	if (status != 0)
		return true;
	return res == stream->machine->globals->value_true;
}

static size_t sos_read(struct stream_obj_stream *stream, void *buf, size_t buf_sz) {
	ow_unused_var(stream), ow_unused_var(buf), ow_unused_var(buf_sz);
	// TODO: Implement read.
	return 0;
}

static size_t sos_write(struct stream_obj_stream *stream, const void *data, size_t size) {
	ow_unused_var(stream), ow_unused_var(data), ow_unused_var(size);
	// TODO: Implement write.
	return 0;
}

static int sos_getc(struct stream_obj_stream *stream) {
	ow_unused_var(stream);
	// TODO: Implement getc.
	return -1;
}

static bool sos_gets(struct stream_obj_stream *stream, char *buf, size_t buf_sz) {
	ow_unused_var(stream), ow_unused_var(buf), ow_unused_var(buf_sz);
	// TODO: Implement gets.
	return false;
}

static int sos_putc(struct stream_obj_stream *stream, int ch) {
	ow_unused_var(stream), ow_unused_var(ch);
	// TODO: Implement putc.
	return -1;
}

static bool sos_puts(struct stream_obj_stream *stream, const char *str) {
	ow_unused_var(stream), ow_unused_var(str);
	// TODO: Implement puts.
	return false;
}

typedef void (*sop_close_t)(struct ow_stream *);
typedef bool (*sop_eof_t)(const struct ow_stream *);
typedef size_t (*sop_read_t)(struct ow_stream *, void *, size_t);
typedef size_t (*sop_write_t)(struct ow_stream *, const void *, size_t);
typedef int (*sop_getc_t)(struct ow_stream *);
typedef bool (*sop_gets_t)(struct ow_stream *, char *, size_t);
typedef int (*sop_putc_t)(struct ow_stream *, int);
typedef bool (*sop_puts_t)(struct ow_stream *, const char *);

static const struct ow_stream_operations sos_operations = {
	.close = NULL,
	.eof   = (sop_eof_t)  sos_eof  ,
	.read  = (sop_read_t) sos_read ,
	.write = (sop_write_t)sos_write,
	.getc  = (sop_getc_t) sos_getc ,
	.gets  = (sop_gets_t) sos_gets ,
	.putc  = (sop_putc_t) sos_putc ,
	.puts  = (sop_puts_t) sos_puts ,
};

int ow_stream_obj_use_stream(
		struct ow_machine *om, struct ow_stream_obj *self,
		int (*func)(void *arg, struct ow_stream *stream), void *func_arg) {
	struct ow_stream *stream;
	struct stream_obj_stream sos;

	struct ow_class_obj *const self_class = ow_object_class(ow_object_from(self));
	if (self_class == om->builtin_classes->file)
		stream = (struct ow_stream *)&((struct ow_file_obj *)self)->file_stream;
	else if (self_class == om->builtin_classes->string_stream)
		stream = (struct ow_stream *)&((struct ow_string_stream_obj *)self)->string_stream;
	else
		stream = (struct ow_stream *)sos_init(&sos, om, self);

	const int ret = func(func_arg, stream);

	if (ow_unlikely(stream == (struct ow_stream *)&sos))
		sos_fini(&sos);

	return ret;
}

static const struct ow_native_func_def stream_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(stream) = {
	.name      = "Stream",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_stream_obj),
	.methods   = stream_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};

struct ow_file_obj *ow_file_obj_new(
		struct ow_machine *om, const ow_path_char_t *path, int flags) {
	struct ow_file_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->file, 0),
		struct ow_file_obj);
	if (ow_file_stream_open(&obj->file_stream, path, flags))
		return obj;
	return NULL;
}

static const struct ow_native_func_def file_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(file) = {
	.name      = "File",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_file_obj),
	.methods   = file_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};

struct ow_string_stream_obj *ow_string_stream_obj_new(struct ow_machine *om) {
	struct ow_string_stream_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->string_stream, 0),
		struct ow_string_stream_obj);
	ow_string_stream_open(&obj->string_stream);
	return obj;
}

static const struct ow_native_func_def string_stream_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(string_stream) = {
	.name      = "StringStream",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_string_stream_obj),
	.methods   = string_stream_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};
