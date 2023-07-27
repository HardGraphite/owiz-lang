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
