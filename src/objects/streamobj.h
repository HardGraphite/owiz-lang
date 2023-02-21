#pragma once

#include "object.h"
#include <utilities/stream.h>

struct ow_machine;

/// Stream object.
struct ow_stream_obj {
	OW_OBJECT_HEAD
};

/// Use the stream or a stream wrap in the stream object.
/// DO NOT use the stream after `func` returns.
int ow_stream_obj_use_stream(
	struct ow_machine *om, struct ow_stream_obj *self,
	int (*func)(void *arg, struct ow_stream *stream), void *func_arg);

/// File stream object.
struct ow_file_obj {
	OW_OBJECT_HEAD
	struct ow_file_stream file_stream;
};

struct ow_file_obj *ow_file_obj_new(
	struct ow_machine *om, const ow_path_char_t *path, int flags);

/// String stream object.
struct ow_string_stream_obj {
	OW_OBJECT_HEAD
	struct ow_string_stream string_stream;
};

struct ow_string_stream_obj *ow_string_stream_obj_new(struct ow_machine *om);
