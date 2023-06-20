#include "objmem.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h> // abort()
#include <string.h>

#include "classobj.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include "smallint.h"
#include <compat/kw_static.h>
#include <machine/machine.h>
#include <utilities/bitset.h>
#include <utilities/memalloc.h>

#include <config/options.h>

#if OW_DEBUG_MEMORY
#    include <stdio.h>
#    include <time.h>
#endif // OW_DEBUG_MEMORY

/* ----- Configurations ----------------------------------------------------- */

#define NON_BIG_SPACE_MAX_ALLOC_SIZE   ((size_t)8 * 1024)
#define NEW_SPACE_CHUNK_SIZE           ((size_t)512 * 1024)
#define OLD_SPACE_CHUNK_SIZE           ((size_t)256 * 1024)
#define BIG_SPACE_THRESHOLD_INIT       ((size_t)16 * NON_BIG_SPACE_MAX_ALLOC_SIZE)

static_assert(NON_BIG_SPACE_MAX_ALLOC_SIZE >= 4 * 1024, "");
static_assert(NON_BIG_SPACE_MAX_ALLOC_SIZE < OLD_SPACE_CHUNK_SIZE / 16, "");

/* ----- Common object operations ------------------------------------------- */

/// Call object's finalizer if it is defined.
#define call_object_finalizer(obj, cls) \
    do {                                \
        void (*finalizer)(struct ow_object *) = ow_class_obj_pub_info(cls)->finalizer; \
        if (ow_unlikely(finalizer))     \
            finalizer(obj);             \
    } while (0)                         \
// ^^^ call_object_finalizer() ^^^

/* ----- Memory span set with function pointer ------------------------------ */

/// A record of span.
struct mem_span_set_node {
    struct mem_span_set_node *_next;
    void *span_addr;
    void (*function)(void);
};

/// A set of memory span.
struct mem_span_set {
    struct mem_span_set_node *_nodes;
};

static_assert(
    offsetof(struct mem_span_set_node, _next)
        == offsetof(struct mem_span_set, _nodes),
    "");

/// Iterate over nodes. Nodes will not be accessed after `STMT`.
#define mem_span_set_foreach_node(set, NODE_VAR, STMT) \
    do {                                               \
        struct mem_span_set_node *__next_node;         \
        for (struct mem_span_set_node *__node = (set)->_nodes; \
            __node; __node = __next_node               \
        ) {                                            \
            __next_node = __node->_next;               \
            { struct mem_span_set_node *NODE_VAR = __node; STMT } \
        }                                              \
    } while (0)                                        \
// ^^^ mem_span_set_foreach_node() ^^^

/// Iterate over nodes and the predecessors.
#define mem_span_set_foreach_node_2(set, PREV_NODE_VAR, NODE_VAR, STMT) \
    do {                                                                \
        struct mem_span_set_node *__node;                               \
        struct mem_span_set_node *PREV_NODE_VAR =                       \
            (struct mem_span_set_node *)(set);                          \
        for (; ; PREV_NODE_VAR = __node) {                              \
            __node = (PREV_NODE_VAR) ->_next;                           \
            if (!__node)                                                \
                break;                                                  \
            { struct mem_span_set_node *NODE_VAR = __node; STMT }       \
        }                                                               \
    } while (0)                                                         \
// ^^^ mem_span_set_foreach_node_2() ^^^

/// Iterate over records.
#define mem_span_set_foreach(set, SPAN_T, SPAN_VAR, FN_T, FN_VAR, STMT) \
    do {                                                                \
        struct mem_span_set_node *__next_node;                          \
        for (struct mem_span_set_node *__node = (set)->_nodes;          \
            __node; __node = __next_node                                \
        ) {                                                             \
            __next_node = __node->_next;                                \
            {                                                           \
                SPAN_T SPAN_VAR = (SPAN_T)__node->span_addr;            \
                FN_T FN_VAR     = (FN_T)__node->function;               \
                STMT                                                    \
            }                                                           \
        }                                                               \
    } while (0)                                                         \
// ^^^ mem_span_set_foreach() ^^^

/// Initialize the set.
static void mem_span_set_init(struct mem_span_set *set) {
    set->_nodes = NULL;
}

/// Finalize the set.
static void mem_span_set_fini(struct mem_span_set *set) {
    mem_span_set_foreach_node(set, node, { ow_free(node); });
    set->_nodes = NULL;
}

/// Search for a record. If found, returns the predecessor node; otherwise returns `NULL`.
static struct mem_span_set_node *mem_span_set_find(
    struct mem_span_set *set, void *span_addr,
    struct mem_span_set_node **predecessor_node /* = NULL */
) {
    mem_span_set_foreach_node_2(set, prev_node, node, {
        if (node->span_addr == span_addr) {
            if (predecessor_node)
                *predecessor_node = prev_node;
            return node;
        }
    });
    return NULL;
}

/// Add a record or update a existing one.
static void mem_span_set_add(
    struct mem_span_set *set,
    void *span_addr, void(*function)(void)
) {
    struct mem_span_set_node *node =
        mem_span_set_find(set, span_addr, NULL);
    if (node) {
        assert(node->span_addr == span_addr);
        node->function = function;
    } else {
        node = ow_malloc(sizeof(struct mem_span_set_node));
        node->_next = set->_nodes;
        node->span_addr = span_addr;
        node->function = function;
        set->_nodes = node;
    }
}

/// Remove a record. Return whether successful.
static bool mem_span_set_remove(struct mem_span_set *set, void *span_addr) {
    struct mem_span_set_node *node, *prev_node;
    node = mem_span_set_find(set, span_addr, &prev_node);
    if (!node)
        return false;
    assert(node == prev_node->_next);
    prev_node->_next = node->_next;
    ow_free(node);
    return true;
}

/* ----- Memory chunk ------------------------------------------------------- */

/// A huge block of memory from where smaller memory block can be allocated.
struct mem_chunk {

    /*
     * +------+-------------+----------+
     * | Meta | Allocated   | Free     |
     * +------+-------------+----------+
     *         ^             ^          ^
     *         _mem       _free      _end
     */

    char             *_free;
    char             *_end;
    struct mem_chunk *_next;
    char              _mem[];
};

/// A `struct mem_chunk` without the member variable `_mem`.
/// Nested flexible array member seems to be invalid. This struct shall only
/// be used as member variable in other struct as a replacement of `struct mem_chunk`.
struct empty_mem_chunk {
    char             *_free;
    char             *_end;
    struct mem_chunk *_next;
};

static_assert(
    sizeof(struct mem_chunk) == sizeof(struct empty_mem_chunk) &&
    offsetof(struct mem_chunk, _free) == offsetof(struct empty_mem_chunk, _free) &&
    offsetof(struct mem_chunk, _end) == offsetof(struct empty_mem_chunk, _end) &&
    offsetof(struct mem_chunk, _next) == offsetof(struct empty_mem_chunk, _next),
    "struct empty_mem_chunk"
);

/// Allocate a chunk (virtual memory).
static struct mem_chunk *mem_chunk_create(size_t size) {
    assert(size > sizeof(struct mem_chunk));
    struct mem_chunk *const chunk = ow_mem_allocate_virtual(size);
    assert(chunk);
    chunk->_free = chunk->_mem;
    chunk->_end  = (char *)chunk + size;
    chunk->_next = NULL;
    return chunk;
}

/// Deallocate a chunk.
static void mem_chunk_destroy(struct mem_chunk *chunk) {
    assert(chunk->_end >= chunk->_mem);
    ow_mem_deallocate_virtual(chunk, (size_t)(chunk->_end - (char *)chunk));
}

/// Allocate from the chunk. On failure (no enough space), returns `NULL`.
ow_forceinline static void *mem_chunk_alloc(struct mem_chunk *chunk, size_t size) {
    assert(size > 0);
    char *const ptr = chunk->_free;
    char *const new_free = ptr + size;
    if (ow_unlikely(new_free >= chunk->_end))
        return NULL;
    chunk->_free = new_free;
    return ptr;
}

/// Forget allocations, i.e., reset the "free" pointer.
static void mem_chunk_forget(struct mem_chunk *chunk) {
    chunk->_free = chunk->_mem;
}

/// Get allocated range (`{&begin, &end}`).
static void mem_chunk_allocated(
    struct mem_chunk *chunk,
    void **region[OW_PARAMARRAY_STATIC 2] /*{&begin, &end}*/
) {
    *region[0] = chunk->_mem;
    *region[1] = chunk->_free;
}

/// Assume all allocations are for objects. Iterate over allocated objects.
#define mem_chunk_foreach_allocated_object(                                    \
    chunk, begin_offset, OBJ_VAR, OBJ_CLASS_VAR, OBJ_SIZE_VAR, STMT            \
)                                                                              \
    do {                                                                       \
        void *allocated_begin, *allocated_end;                                 \
        mem_chunk_allocated(chunk, (void **[]){&allocated_begin, &allocated_end}); \
        allocated_begin = (char *)allocated_begin + begin_offset;              \
        size_t __obj_size;                                                     \
        for (struct ow_object *__this_obj = allocated_begin;                   \
            (void *)__this_obj < allocated_end;                                \
            __this_obj = (struct ow_object *)((char *)__this_obj + __obj_size) \
        ) {                                                                    \
            struct ow_class_obj *const __obj_class = ow_object_class(__this_obj);  \
            __obj_size = ow_class_obj_object_size(__obj_class, __this_obj);    \
            {                                                                  \
                struct ow_object *const OBJ_VAR = __this_obj;                  \
                struct ow_class_obj *const OBJ_CLASS_VAR = __obj_class;        \
                const size_t OBJ_SIZE_VAR = __obj_size;                        \
                STMT                                                           \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \
// ^^^ new_space_foreach_allocated() ^^^

/// A list of `struct mem_chunk`.
struct mem_chunk_list {

    /*
     * Node-0    Node-1   ...    Last node
     * +-+--+   +-+--+           +-+--+
     * | | -+-->| | -+--> ... -->| | -+--> X
     * +-+--+   +-+--+           +-+--+
     *  ^                         ^
     *  _head.next                _tail
     */

    struct mem_chunk *_tail;
    struct empty_mem_chunk _head;
};

/// Iterate over chunks.
#define mem_chunk_list_foreach(LIST, CHUNK_VAR, STMT) \
    do {                                              \
        struct mem_chunk *__chunk;                    \
        struct mem_chunk *__prev_chunk = _mem_chunk_list_head((LIST));\
        for (; ; __prev_chunk = __chunk) {            \
            __chunk = __prev_chunk->_next;            \
            if (!__chunk)                             \
                break;                                \
            {                                         \
                struct mem_chunk *CHUNK_VAR = __chunk;\
                STMT                                  \
            }                                         \
        }                                             \
    } while (0)                                       \
// ^^^ mem_chunk_list_foreach() ^^^

static void _mem_chunk_list_del_from(struct mem_chunk *chunk) {
    while (chunk) {
        struct mem_chunk *const next = chunk->_next;
        mem_chunk_destroy(chunk);
        chunk = next;
    }
}

ow_forceinline static struct mem_chunk *
_mem_chunk_list_head(struct mem_chunk_list *list) {
    return (struct mem_chunk *)(&list->_head);
}

/// Initialize the list.
static void mem_chunk_list_init(struct mem_chunk_list *list) {
    list->_tail = _mem_chunk_list_head(list);
    list->_head._free = _mem_chunk_list_head(list)->_mem;
    list->_head._end  = _mem_chunk_list_head(list)->_mem;
    list->_head._next = NULL;
    assert(!mem_chunk_alloc(_mem_chunk_list_head(list), 1));
}

/// Delete all chunks in the list.
static void mem_chunk_list_fini(struct mem_chunk_list *list) {
    _mem_chunk_list_del_from(list->_head._next);
    list->_head._next = NULL;
}

/// Get first chunk.
static struct mem_chunk *mem_chunk_list_front(struct mem_chunk_list *list) {
    return _mem_chunk_list_head(list);
}

/// Get last chunk.
static struct mem_chunk *mem_chunk_list_back(struct mem_chunk_list *list) {
    return list->_tail;
}

/// Check if the list contains the given chunk.
ow_unused_func static bool mem_chunk_list_contains(
    struct mem_chunk_list *list, const struct mem_chunk *chunk
) {
    mem_chunk_list_foreach(list, c, {
        if (c == chunk)
            return true;
    });
    return false;
}

/// Add chunk to the list tail.
static void mem_chunk_list_append(
    struct mem_chunk_list *list, struct mem_chunk *chunk
) {
    assert(!chunk->_next);
    assert(!list->_tail->_next);
    list->_tail->_next = chunk;
    list->_tail = chunk;
}

/// Remove chunks after the given one.
static void mem_chunk_list_pop_after(
    struct mem_chunk_list *list, const struct mem_chunk *after_chunk
) {
    assert(mem_chunk_list_contains(list, after_chunk));
    list->_tail = (struct mem_chunk *)after_chunk;
    list->_tail->_next = NULL;
}

/// Create chunk and add it to the list tail.
static struct mem_chunk *mem_chunk_list_append_created(
    struct mem_chunk_list *list, size_t chunk_size
) {
    struct mem_chunk *const chunk = mem_chunk_create(chunk_size);
    mem_chunk_list_append(list, chunk);
    return chunk;
}

/// Delete chunks after the given one.
static void mem_chunk_list_destroy_after(
    struct mem_chunk_list *list, const struct mem_chunk *after_chunk
) {
    struct mem_chunk *const first_chunk = after_chunk->_next;
    mem_chunk_list_pop_after(list, after_chunk);
    _mem_chunk_list_del_from(first_chunk);
}

/* ----- Big space (old generation, large objects) -------------------------- */

/*
 * In big space, mark-sweep GC algorithm is used.
 * All allocated objects are put in a linked list.
 * The `PTR` field in object meta stores the next object in the list.
 * `PTR & 0b0100` indicates whether this object contains references to young objects.
 */

struct _big_space_head {
    OW_OBJECT_HEAD
};

static_assert(
    sizeof(struct _big_space_head) == sizeof(struct ow_object),
    "struct _big_space_head"
);

/// Big space manager.
struct big_space {
    size_t allocated_size;
    size_t threshold_size;
    struct _big_space_head _head; // Fake object.
};

ow_forceinline static struct ow_object *_big_space_head(struct big_space *space) {
    return (struct ow_object *)&space->_head;
}

#define big_space_make_meta_ptr_data(NEXT_OBJ, YOUNG_REF) \
    ( assert(!((uintptr_t)NEXT_OBJ & 7)), ((uintptr_t)NEXT_OBJ | (YOUNG_REF ? 4 : 0)) )

#define big_space_unpack_meta_ptr_data(PTR_DATA, NEXT_OBJ_VAR, YOUNG_REF_VAR) \
    do {                                                                      \
        NEXT_OBJ_VAR  = (struct ow_object *)(PTR_DATA & ~(uintptr_t)7);       \
        YOUNG_REF_VAR = PTR_DATA & 7;                                         \
    } while (0)

/// Iterate over objects. Object will not be access after `STMT`.
#define big_space_foreach(space, OBJ_VAR, OBJ_HAS_YOUNG_VAR, STMT) \
    do {                                                           \
        struct ow_object *__obj, *__next_obj;                      \
        bool __young_ref;                                          \
        for (__obj = _big_space_get_first((space)); __obj; __obj = __next_obj) { \
            big_space_unpack_meta_ptr_data(                        \
                ow_object_meta_load_(PTR, uintptr_t, __obj->_meta),\
                __next_obj, __young_ref                            \
            );                                                     \
            {                                                      \
                struct ow_object *const OBJ_VAR = __obj;           \
                const bool OBJ_HAS_YOUNG_VAR = __young_ref;        \
                STMT                                               \
            }                                                      \
        }                                                          \
    } while (0)                                                    \
// ^^^ big_space_foreach() ^^^

/// Iterate over objects. Providing the predecessors.
#define big_space_foreach_2(space, PREV_OBJ_VAR, OBJ_VAR, STMT) \
    do {                                                        \
        struct ow_object *__prev_obj, *__this_obj;              \
        bool __young_ref;                                       \
        for (__prev_obj = _big_space_head(space); ; __prev_obj = __this_obj) { \
            big_space_unpack_meta_ptr_data(                     \
                ow_object_meta_load_(PTR, uintptr_t, __prev_obj->_meta),\
                __this_obj, __young_ref                         \
            );                                                  \
            ow_unused_var(__young_ref);                         \
            if (ow_unlikely(!__this_obj))                       \
                break;                                          \
            {                                                   \
                struct ow_object *const PREV_OBJ_VAR = __prev_obj;      \
                struct ow_object *const OBJ_VAR      = __this_obj;      \
                STMT                                            \
            }                                                   \
        }                                                       \
    } while (0)                                                 \
// ^^^ big_space_foreach_2() ^^^

static struct ow_object *_big_space_get_first(struct big_space *space) {
    const uintptr_t ptr_data =
        ow_object_meta_load_(PTR, uintptr_t, space->_head._meta);
    assert(!(ptr_data & 7));
    return (struct ow_object *)ptr_data;
}

static void _big_space_set_first(struct big_space *space, struct ow_object *obj) {
    ow_object_meta_store_(PTR,
        space->_head._meta,
        big_space_make_meta_ptr_data(obj, false)
    );
}

/// Initialize space.
static void big_space_init(struct big_space *space) {
    space->allocated_size = 0U;
    space->threshold_size = BIG_SPACE_THRESHOLD_INIT;
    ow_object_meta_init(space->_head._meta, true, true, 0U, NULL);
}

/// Finalize allocated objects and the space.
static void big_space_fini(struct big_space *space) {
    big_space_foreach(space, obj, has_young, {
        ow_unused_var(has_young);
        call_object_finalizer(obj, ow_object_class(obj));
        ow_free(obj);
    });
}

#if OW_DEBUG_MEMORY

static void big_space_print_usage(struct big_space *space, FILE *stream) {
    fprintf(
        stream, "<BigSpc threshold_size=\"%zu\" allocated_size=\"%zu\">\n",
        space->threshold_size , space->allocated_size
    );
    big_space_foreach(space, obj, has_young, {
        fprintf(
            stream,
            "  <obj addr=\"%p\" has_young=\"%s\" />\n",
            (void *)obj,
            has_young ? "yes" : "no"
        );
    });
    fputs("</BigSpc>\n", stream);
}

#endif // OW_DEBUG_MEMORY

/// Allocate storage for an object. On failure, returns `NULL`.
ow_forceinline static struct ow_object *
big_space_alloc(struct big_space *space, void *class_, size_t size) {
    assert(size >= sizeof(struct ow_object_meta));
    const size_t new_allocated_size = space->allocated_size + size;
    if (ow_unlikely(new_allocated_size > space->threshold_size))
        return NULL;
    space->allocated_size = new_allocated_size;
    struct ow_object *const obj = ow_malloc(size);
    const uintptr_t ptr_data =
        big_space_make_meta_ptr_data(_big_space_get_first(space), false);
    _big_space_set_first(space, obj);
    assert(ow_object_meta_check_value(ptr_data));
    assert(ow_object_meta_check_value(class_));
    ow_object_meta_init(obj->_meta, true, true, ptr_data, class_);
    return obj;
}

/// Write barrier: mark object containing young reference.
ow_forceinline static void big_space_remember_object(struct ow_object *obj) {
    struct ow_object *next_obj;
    bool orig_young_ref;
    big_space_unpack_meta_ptr_data(
        ow_object_meta_load_(PTR, uintptr_t, obj->_meta),
        next_obj, orig_young_ref
    );
    if (!orig_young_ref) {
        uintptr_t new_ptr_data = big_space_make_meta_ptr_data(next_obj, true);
        assert(ow_object_meta_check_value(new_ptr_data));
        ow_object_meta_store_(PTR, obj->_meta, new_ptr_data);
    }
}

/// Fast GC: mark young fields of remembered objects. Return number of found objects.
static size_t big_space_mark_remembered_objects_young_fields(
    struct big_space *space
) {
    size_t count = 0;
    big_space_foreach(space, obj, has_young, {
        if (ow_unlikely(has_young)) {
            count++;
            assert(ow_object_meta_test_(OLD, obj->_meta));
            _ow_objmem_mark_old_referred_object_young_fields_rec(obj);
        }
    });
    return count;
}

/// Fast GC: Update references in remembered objects and clear the remembered flags.
static size_t big_space_update_remembered_objects_references_and_forget_remembered_objects(
    struct big_space *space, size_t hint_max_count
) {
    size_t count = 0;
    big_space_foreach(space, obj, has_young, {
        if (ow_unlikely(count >= hint_max_count))
            break;
        if (ow_unlikely(has_young)) {
            count++;
            // Update reference.
            _ow_objmem_move_object_fields(obj);
            // Clear remembered flag.
            void *const next_obj = __next_obj; // `__next_obj` is defined in `big_space_foreach()`.
            ow_object_meta_store_(PTR,
                obj->_meta,
                big_space_make_meta_ptr_data(next_obj, false)
            );
        }
    });
    return count;
}

/// Full GC: delete unreachable objects and clear flags of reachable objects
/// (including GC marks and remembered flags).
static void big_space_delete_unreachable_objects_and_reset_reachable_objects(
    struct big_space *space
) {
    size_t deleted_size = 0;

    big_space_foreach_2(space, prev_obj, obj, {
        void *next_obj;
        bool obj_has_young;
        big_space_unpack_meta_ptr_data(
            ow_object_meta_load_(PTR, uintptr_t, obj->_meta),
            next_obj, obj_has_young
        );
        if (ow_likely(ow_object_meta_test_(MRK, obj->_meta))) {
            // Clear mark.
            ow_object_meta_reset_(MRK, obj->_meta);
            // Clear remembered flag.
            if (ow_unlikely(obj_has_young)) {
                ow_object_meta_store_(PTR,
                    obj->_meta,
                    big_space_make_meta_ptr_data(next_obj, false)
                );
            }
        } else {
            // Delete object.
            struct ow_class_obj *const obj_class = ow_object_class(obj);
            call_object_finalizer(obj, obj_class);
            deleted_size += ow_class_obj_object_size(obj_class, obj);
            ow_free(obj);
            // Remove list node.
            ow_object_meta_store_(PTR,
                prev_obj->_meta,
                big_space_make_meta_ptr_data(next_obj, false)
            );
            // Backward.
            __this_obj = __prev_obj; // `__this_obj` and `__prev_obj` are define in `big_space_foreach_2()`.
            // FIXME: This is ugly.
        }
    });

    assert(deleted_size <= space->allocated_size);
    space->allocated_size -= deleted_size;
}

/// Full GC: update references to objects. Unreachable objects shall have been deleted.
static void big_space_update_references(struct big_space *space) {
    big_space_foreach(space, obj, has_young, {
        ow_unused_var(has_young);
        _ow_objmem_move_object_fields(obj);
    });
}

#if OW_DEBUG_MEMORY

static int big_space_post_gc_check(struct big_space *space) {
    big_space_foreach(space, obj, has_young, {
        if (has_young)
            return -1;
        if (!(ow_object_meta_test_(OLD, obj->_meta) && ow_object_meta_test_(BIG, obj->_meta)))
            return -2;
        if (ow_object_meta_test_(MRK, obj->_meta))
            return -3;
    });
    return 0;
}

#endif // OW_DEBUG_MEMORY

/* ----- Old space (old generation) ----------------------------------------- */

/*
 * In old space, mark-compact GC algorithm is used.
 * Object storage is allocated from chunks, while the chunks are put in a list.
 * The PTR field in object meta stores a pointer to chunk meta when GC is not running.
 * A remembered set is available for each chunk (a pointer at the beginning of chunk)
 * indicating which objects in this chunk contains references to young objects.
 */

#define OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS 1024
#define OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_SIZE \
    ow_bitset_required_size(OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS)

/// Remembered set for a chunk. It records offsets in the chunk.
struct old_space_chunk_remembered_set {

    /*
     * +-------+
     * |bucket |
     * | _count|
     * +-------+
     * |buckets|
     * |       |   +----------------------------+
     * | [0] ----->| bitset, `BUCKET_BITS` bits |
     * |       |   +----------------------------+
     * | [1] ----->(NULL, empty)
     * |       |
     * | [2] ----->(NULL, empty)
     * |       |
     * |  ...  |    ...
     *
     */

    size_t            _bucket_count;
    struct ow_bitset *_buckets[];
};

/// Create an empty remembered set.
static struct old_space_chunk_remembered_set *
old_space_chunk_remembered_set_create(size_t chunk_size) {
    const size_t bucket_count =
        chunk_size / sizeof(void *) / OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS;
    struct old_space_chunk_remembered_set *const set = ow_malloc(
        sizeof(struct old_space_chunk_remembered_set)
            + sizeof(struct ow_bitset *) * bucket_count
    );
    set->_bucket_count = bucket_count;
    memset(set->_buckets, 0, sizeof(struct ow_bitset *) * bucket_count);
    return set;
}

/// Delete a remembered set.
static void old_space_chunk_remembered_set_destroy(
    struct old_space_chunk_remembered_set *set
) {
    for (size_t i = 0, n = set->_bucket_count; i < n; i++) {
        struct ow_bitset *const b = set->_buckets[i];
        ow_free(b);
    }
    ow_free(set);
}

/// Record an offset.
ow_forceinline static void old_space_chunk_remembered_set_record(
    struct old_space_chunk_remembered_set *set, size_t offset
) {
    assert(!(offset & (sizeof(void *) - 1)));
    offset /= sizeof(void *);
    const size_t bucket_index = offset / OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS;
    const size_t bit_index    = offset % OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS;
    assert(bucket_index < set->_bucket_count);
    struct ow_bitset *bucket = set->_buckets[bucket_index];
    if (ow_unlikely(!bucket)) {
        bucket = ow_malloc(OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_SIZE);
        ow_bitset_clear(bucket, OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_SIZE);
        set->_buckets[bucket_index] = bucket;
    }
    ow_bitset_try_set_bit(bucket, bit_index);
}

/// Iterate over records.
#define old_space_chunk_remembered_set_foreach(SET_PTR, OFFSET_VAR, STMT) \
    do {                                                                  \
        struct old_space_chunk_remembered_set *const set = (SET_PTR);     \
        for (size_t i = 0, n = set->_bucket_count; i < n; i++) {          \
            struct ow_bitset *const bucket = set->_buckets[i];            \
            if (!bucket)                                                  \
                continue;                                                 \
            const size_t offset_base =                                    \
                i * OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_BITS * sizeof(void *); \
            ow_bitset_foreach_set(                                        \
                bucket, OLD_SPACE_CHUNK_REMEMBERED_SET_BUCKET_SIZE, bit_index,   \
            {                                                             \
                const size_t OFFSET_VAR =                                 \
                    offset_base | bit_index * sizeof(void *);             \
                { STMT }                                                  \
            });                                                           \
        }                                                                 \
    } while (0)                                                           \
// ^^^ old_space_chunk_remembered_set_foreach() ^^^

/// Old space manager.
struct old_space {
    struct mem_chunk_list _chunks;
};

/// Meta data of a old space chunk.
/// Must be the first block of memory allocated from the chunk.
struct old_space_chunk_meta {
    struct old_space_chunk_remembered_set *remembered_set; // Nullable.
    void *iter_visited_end; // Nullable.
};

/// Initialize chunk meta.
static void old_space_chunk_meta_init(struct old_space_chunk_meta *meta) {
    meta->remembered_set = NULL;
    meta->iter_visited_end = NULL;
}

/// Finalize chunk meta.
static void old_space_chunk_meta_fini(struct old_space_chunk_meta *meta) {
    if (meta->remembered_set)
        old_space_chunk_remembered_set_destroy(meta->remembered_set);
}

#define old_space_chunk_meta_addr(CHUNK_PTR) \
    ((struct old_space_chunk_meta *)&((CHUNK_PTR)->_mem[0]))

#define old_space_chunk_meta_of_obj(OBJ_PTR) \
    (ow_object_meta_load_(PTR, struct old_space_chunk_meta *, (OBJ_PTR)->_meta))

#define old_space_chunk_of_meta(META_PTR) \
    ((struct mem_chunk *)((char *)(META_PTR) - offsetof(struct mem_chunk, _mem)))

#define old_space_chunk_first_obj(CHUNK_PTR) \
    ((struct ow_object *) \
        ((char *)old_space_chunk_meta_addr(CHUNK_PTR) \
            + sizeof(struct old_space_chunk_meta)))

/// Old space storage iterator. Invalidated after de-allocations in old space.
struct old_space_iterator {
    struct mem_chunk *chunk;
    void             *point; // Position in the chunk.
};

/// Make an iterator at the first allocated object.
struct old_space_iterator old_space_allocated_begin(struct old_space *space) {
    struct mem_chunk *const first_chunk = mem_chunk_list_front(&space->_chunks);
    return (struct old_space_iterator){
        .chunk = first_chunk,
        .point = old_space_chunk_first_obj(first_chunk),
    };
}

/// Make an iterator after the last allocated object.
struct old_space_iterator old_space_allocated_end(struct old_space *space) {
    struct mem_chunk *const last_chunk = mem_chunk_list_back(&space->_chunks);
    return (struct old_space_iterator){
        .chunk = last_chunk,
        .point = last_chunk->_free,
    };
}

/// Move iterator forward `size` bytes. Return the old value of `iter->point`.
/// Return `NULL` if reaches the end of last chunk.
ow_forceinline static void *old_space_iterator_forward(
    struct old_space_iterator *iter, size_t size
) {
    struct mem_chunk *chunk = iter->chunk;
    void *point = iter->point;
    char *new_point = (char *)point + size;
    if (ow_unlikely(new_point >= chunk->_end)) {
        struct mem_chunk *const next_chunk = chunk->_next;
        if (!next_chunk)
            return NULL;
        // Record the last visited position of current chunk.
        struct old_space_chunk_meta *const orig_chunk_meta =
            old_space_chunk_meta_addr(chunk);
        if ((void *)&orig_chunk_meta->iter_visited_end < (void *)chunk->_end) {
            assert(!orig_chunk_meta->iter_visited_end);
            orig_chunk_meta->iter_visited_end = point;
        } else {
            // The `_head` of `struct mem_chunk_list` is an empty chunk and has no meta.
            // Skip if `chunk` is the empty one.
            assert(chunk->_mem == chunk->_end);
        }
        // Go to next chunk.
        chunk = next_chunk;
        point = old_space_chunk_first_obj(chunk);
        new_point = (char *)point + size;
        assert(new_point < chunk->_end);
        iter->chunk = chunk;
    }
    iter->point = new_point;
    return point;
}

static struct mem_chunk *old_space_add_chunk(struct old_space *);

/// Initialize space.
static void old_space_init(struct old_space *space) {
    mem_chunk_list_init(&space->_chunks);
    old_space_add_chunk(space);
}

/// Finalize allocated objects and delete remembered sets, but do not free storage.
static void old_space_pre_fini(struct old_space *space) {
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        old_space_chunk_meta_fini(old_space_chunk_meta_addr(chunk));
        mem_chunk_foreach_allocated_object(
            chunk, sizeof(struct old_space_chunk_meta), obj, obj_class, obj_size,
        {
            ow_unused_var(obj_size);
            call_object_finalizer(obj, obj_class);
        });
    });
}

/// Finalize space. `old_space_pre_fini()` must have been called.
static void old_space_fini(struct old_space *space) {
    mem_chunk_list_fini(&space->_chunks);
}

/// Add a chunk to the end of list.
ow_noinline static struct mem_chunk *old_space_add_chunk(struct old_space *space) {
    const size_t chunk_size = OLD_SPACE_CHUNK_SIZE;
    struct  mem_chunk *const chunk =
        mem_chunk_list_append_created(&space->_chunks, chunk_size);
    struct old_space_chunk_meta *const chunk_meta =
        mem_chunk_alloc(chunk, sizeof(struct old_space_chunk_meta));
    assert(chunk_meta);
    assert(chunk_meta == old_space_chunk_meta_addr(chunk));
    old_space_chunk_meta_init(chunk_meta);
    return chunk;
}

/// Delete chunks after the given one.
static void old_space_remove_chunks_after(
    struct old_space *space, struct mem_chunk *after_chunk
) {
    for (struct mem_chunk *chunk = after_chunk->_next; chunk; chunk = chunk->_next)
        old_space_chunk_meta_fini(old_space_chunk_meta_addr(chunk));
    mem_chunk_list_destroy_after(&space->_chunks, after_chunk);
}

#if OW_DEBUG_MEMORY

static void old_space_print_usage(struct old_space *space, FILE *stream) {
    fputs("<OldSpc>\n", stream);
    size_t chunk_index = 0;
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        const size_t chunk_mem_size = (size_t)(chunk->_end - chunk->_mem);
        const size_t chunk_free_size = (size_t)(chunk->_end - chunk->_free);
        struct old_space_chunk_remembered_set *const r_set =
            old_space_chunk_meta_addr(chunk)->remembered_set;
        fprintf(
            stream, "  <chunk id=\"%zu\" addr=\"%p\" size=\"%zu\" free_size=\"%zu\" has_r_set=\"%s\" />\n",
            chunk_index, (void *)chunk, chunk_mem_size, chunk_free_size, r_set ? "yes" : "no"
        );
        if (r_set) {
            fprintf(stream, "  <r_set id=\"%zu\" addr=\"%p\">", chunk_index, (void *)r_set);
            old_space_chunk_remembered_set_foreach(r_set, offset, {
                fprintf(stream, " %zu", offset);
            });
            fputs(" </r_set>\n", stream);
        }
        chunk_index++;
    });
    fputs("</OldSpc>\n", stream);
}

#endif // OW_DEBUG_MEMORY

/// Allocate storage for an object. On failure, returns `NULL`.
ow_forceinline static struct ow_object *
old_space_alloc(struct old_space *space, void *class_, size_t size) {
    assert(size >= sizeof(struct ow_object_meta));
    struct mem_chunk *const chunk = mem_chunk_list_back(&space->_chunks);
    struct ow_object *const obj = mem_chunk_alloc(chunk, size);
    if (ow_unlikely(!obj))
        return NULL;
    void *const meta_addr = old_space_chunk_meta_addr(chunk);
    assert(ow_object_meta_check_value(meta_addr));
    assert(ow_object_meta_check_value(class_));
    ow_object_meta_init(obj->_meta, true, false, meta_addr, class_);
    return obj;
}

/// Full GC: move iterator to reserve storage. Allocate new chunk if there is
/// no enough storage. Return the storage, which is not initialized. The space
/// state is not modified.
ow_forceinline static void *old_space_fake_alloc(
    struct old_space *space, struct old_space_iterator *alloc_pos, size_t size
) {
    void *ptr;
retry:
    ptr = old_space_iterator_forward(alloc_pos, size);
    if (ow_unlikely(!ptr)) {
        assert(alloc_pos->chunk == mem_chunk_list_back(&space->_chunks));
        old_space_add_chunk(space);
        assert(alloc_pos->chunk->_next == mem_chunk_list_back(&space->_chunks));
        goto retry;
    }
    return ptr;
}

/// Full GC: delete unused chunks and update `_free` pointer of each chunk.
/// The iterator `trunc_from` can only be modified by `old_space_fake_alloc()`
/// before calling this function.
static void old_space_truncate(
    struct old_space *space, struct old_space_iterator trunc_from
) {
    // TODO: cache unused chunks instead of deleting them.
    old_space_remove_chunks_after(space, trunc_from.chunk);

    assert(space->_chunks._tail == trunc_from.chunk);
    assert(!old_space_chunk_meta_addr(trunc_from.chunk)->iter_visited_end);
    old_space_chunk_meta_addr(trunc_from.chunk)->iter_visited_end = trunc_from.point;

    mem_chunk_list_foreach(&space->_chunks, chunk, {
        struct old_space_chunk_meta *const chunk_meta =
            old_space_chunk_meta_addr(chunk);
        void *const new_free_pos = chunk_meta->iter_visited_end;
        chunk_meta->iter_visited_end = NULL;
        assert(new_free_pos);
        assert(new_free_pos > (void *)chunk->_mem
            && new_free_pos < (void *)chunk->_end);
        chunk->_free = new_free_pos;
    });
}

/// Full GC: reallocate storages for survivors and clear remembered set.
/// Reallocated objects are neither initialized nor moved. Pointer to new storage
/// is written to the PTR field of object meta. Also call finalizers of dead
/// objects if there are.
static void old_space_realloc_survivors_and_forget_remembered_objects(
    struct old_space *space, struct old_space_iterator *realloc_iter
) {
    // To avoid overlapping and minimize movements, the iterator must be at the
    // beginning of available spaces.
    assert(realloc_iter->chunk == mem_chunk_list_front(&space->_chunks)
        && realloc_iter->point == old_space_chunk_first_obj(realloc_iter->chunk));

    mem_chunk_list_foreach(&space->_chunks, chunk, {
        // Delete remembered set.
        struct old_space_chunk_meta *const chunk_meta = old_space_chunk_meta_addr(chunk);
        if (chunk_meta->remembered_set) {
            old_space_chunk_remembered_set_destroy(chunk_meta->remembered_set);
            chunk_meta->remembered_set = NULL;
        }
        // Update references.
        mem_chunk_foreach_allocated_object(
            chunk, sizeof(struct old_space_chunk_meta), obj, obj_class, obj_size,
        {
            ow_unused_var(obj_class);
            if (ow_unlikely(!ow_object_meta_test_(MRK, obj->_meta))) {
                call_object_finalizer(obj, obj_class);
                continue;
            }
            void *const new_mem =
                old_space_fake_alloc(space, realloc_iter, obj_size);
            assert(new_mem);
            assert(ow_object_meta_check_value(new_mem));
            ow_object_meta_store_(PTR, obj->_meta, new_mem);
        });
    });
}

/// Write barrier: record object in remembered set.
ow_forceinline static void old_space_add_remembered_object(
    struct old_space_chunk_meta *chunk_meta, struct ow_object *obj
) {
    assert(
        (void *)chunk_meta < (void *)obj &&
        (void *)old_space_chunk_of_meta(chunk_meta)->_end > (void *)obj);
    struct old_space_chunk_remembered_set *r_set = chunk_meta->remembered_set;
    if (ow_unlikely(!r_set)) {
        struct mem_chunk *const chunk = old_space_chunk_of_meta(chunk_meta);
        const size_t chunk_size = (size_t)(chunk->_end - (char *)chunk);
        r_set = old_space_chunk_remembered_set_create(chunk_size);
        chunk_meta->remembered_set = r_set;
    }
    old_space_chunk_remembered_set_record(
        r_set,
        (size_t)((char *)obj - (char *)chunk_meta)
    );
}

/// Fast GC: mark young fields of recorded objects in remembered set.
/// Return the number of involved chunks.
static size_t old_space_mark_remembered_objects_young_fields(struct old_space *space) {
    size_t count = 0;
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        struct old_space_chunk_meta *const chunk_meta =
            old_space_chunk_meta_addr(chunk);
        struct old_space_chunk_remembered_set *const r_set =
            chunk_meta->remembered_set;
        if (ow_likely(!r_set))
            continue;
        count++;
        old_space_chunk_remembered_set_foreach(r_set, obj_offset, {
            struct ow_object *const obj =
                (struct ow_object *)((char *)chunk_meta + obj_offset);
            assert(ow_object_meta_test_(OLD, obj->_meta));
            _ow_objmem_mark_old_referred_object_young_fields_rec(obj);
        });
    });
    return count;
}

/// Fast GC: update references in a region starting from `begin`.
static void old_space_update_references_from(
    struct old_space *space,
    struct old_space_iterator begin
) {
    ow_unused_var(space);
    assert(mem_chunk_list_contains(&space->_chunks, begin.chunk));
    assert(begin.point > (void *)begin.chunk->_mem);
    struct mem_chunk *chunk = begin.chunk;
    size_t chunk_start_offset = (size_t)((char *)begin.point - begin.chunk->_mem);

    while (chunk) {
        mem_chunk_foreach_allocated_object(
            chunk, chunk_start_offset, obj, obj_class, obj_size,
        {
            (ow_unused_var(obj_class), ow_unused_var(obj_size));
            _ow_objmem_move_object_fields(obj);
        });

        chunk = chunk->_next;
        chunk_start_offset = sizeof(struct old_space_chunk_meta);
    }
}

/// GC: update references in recorded objects in remembered set.
static size_t old_space_update_remembered_objects_references_forget_remembered_objects(
    struct old_space *space, size_t hint_max_count
) {
    size_t count = 0;
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        if (ow_unlikely(count >= hint_max_count))
            break;
        struct old_space_chunk_meta *const chunk_meta =
            old_space_chunk_meta_addr(chunk);
        struct old_space_chunk_remembered_set *const r_set =
            chunk_meta->remembered_set;
        if (ow_likely(!r_set))
            continue;
        count++;
        // Update references.
        old_space_chunk_remembered_set_foreach(r_set, obj_offset, {
            struct ow_object *const obj =
                (struct ow_object *)((char *)chunk_meta + obj_offset);
            _ow_objmem_move_object_fields(obj);
        });
        // Delete remembered set.
        old_space_chunk_remembered_set_destroy(r_set);
        chunk_meta->remembered_set = NULL;
    });
    return count;
}

/// Full GC: update references to objects. References in unmarked objects are skipped.
static void old_space_update_references(struct old_space *space) {
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        mem_chunk_foreach_allocated_object(
            chunk, sizeof(struct old_space_chunk_meta), obj, obj_class, obj_size,
        {
            (ow_unused_var(obj_class), ow_unused_var(obj_size));
            if (ow_unlikely(!ow_object_meta_test_(MRK, obj->_meta)))
                continue;
            _ow_objmem_move_object_fields(obj);
        });
    });
}

struct old_space_init_reallocated_obj_meta_context {
    struct old_space_chunk_meta *this_chunk_meta;
    struct mem_chunk            *this_chunk;
    void                        *this_chunk_end;
    struct old_space            *space;
};

/// Make a `struct old_space_init_reallocated_obj_meta_context`.
static struct old_space_init_reallocated_obj_meta_context
old_space_init_reallocated_obj_meta_context(struct old_space *space) {
    const struct old_space_iterator begin = old_space_allocated_begin(space);
    return (struct old_space_init_reallocated_obj_meta_context){
        .this_chunk_meta = old_space_chunk_meta_addr(begin.chunk),
        .this_chunk      = begin.chunk,
        .this_chunk_end  = begin.chunk->_end,
        .space           = space,
    };
}

ow_noinline static void _old_space_init_reallocated_obj_meta_slow(
    struct old_space_init_reallocated_obj_meta_context *ctx,
    struct ow_object *obj, void *obj_class
) {
    assert(!(
        (void *)obj > (void *)ctx->this_chunk &&
        (void *)obj < ctx->this_chunk_end
    ));

    for (struct mem_chunk *chunk = ctx->this_chunk->_next; chunk; chunk = chunk->_next) {
        if ((void *)obj > (void *)chunk && (void *)obj < (void *)chunk->_end) {
            assert(obj >= old_space_chunk_first_obj(chunk));

            struct old_space_chunk_meta *const chunk_meta =
                old_space_chunk_meta_addr(chunk);

            ctx->this_chunk_meta = chunk_meta;
            ctx->this_chunk = chunk;
            ctx->this_chunk_end = chunk->_end;

            assert(ow_object_meta_check_value((void *)chunk_meta));
            ow_object_meta_init(obj->_meta, true, false, chunk_meta, obj_class);
            return;
        }
    }

    abort(); // Function misused. No matching chunk found after the given one.
}

/// Initialize object meta whose storage is allocated with `old_space_fake_alloc()`.
/// The order of calling this function must be the same with that of calling
/// `old_space_fake_alloc()`.
ow_forceinline static void old_space_init_reallocated_obj_meta(
    struct old_space_init_reallocated_obj_meta_context *ctx,
    struct ow_object *obj, void *obj_class
) {
    if (ow_likely(
        (void *)obj > (void *)ctx->this_chunk &&
        (void *)obj < ctx->this_chunk_end
    )) {
        void *const ptr = ctx->this_chunk_meta;
        assert(ow_object_meta_check_value(ptr));
        ow_object_meta_init(obj->_meta, true, false, ptr, obj_class);
    } else {
        _old_space_init_reallocated_obj_meta_slow(ctx, obj, obj_class);
    }
}

/// Full GC: move objects whose storages are reallocated with function
/// `old_space_realloc_survivors()`.
static void old_space_move_reallocated_objects(
    struct old_space *space,
    struct old_space_init_reallocated_obj_meta_context *ctx
) {
    // Like what is stated in `old_space_realloc_survivors()`, the order matters.
    assert(ctx->this_chunk == mem_chunk_list_front(&space->_chunks));

    mem_chunk_list_foreach(&space->_chunks, chunk, {
        mem_chunk_foreach_allocated_object(
            chunk, sizeof(struct old_space_chunk_meta), obj, obj_class, obj_size,
        {
            if (ow_unlikely(!ow_object_meta_test_(MRK, obj->_meta)))
                continue;

            ow_object_meta_reset_(MRK, obj->_meta);

            struct ow_object *const new_obj =
                ow_object_meta_load_(PTR, struct ow_object *, obj->_meta);

            if (obj == new_obj) {
                // The storage address is not changed. There is no doubt that
                // chunk_meta is the meta of current chunk.
                void *const ptr = old_space_chunk_meta_addr(chunk);
                assert(ow_object_meta_check_value(ptr));
                ow_object_meta_init(obj->_meta, true, false, ptr, obj_class);
                continue;
            }

            old_space_init_reallocated_obj_meta(ctx, new_obj, obj_class);

            // May overlap. DO NOT use `memcpy()`.
            memmove(
                (char *)new_obj + OW_OBJECT_HEAD_SIZE,
                (char *)obj + OW_OBJECT_HEAD_SIZE,
                obj_size - OW_OBJECT_HEAD_SIZE
            );
        });
    });
}

#if OW_DEBUG_MEMORY

static int old_space_post_gc_check(struct old_space *space) {
    mem_chunk_list_foreach(&space->_chunks, chunk, {
        struct old_space_chunk_meta *const chunk_meta = old_space_chunk_meta_addr(chunk);
        if (chunk_meta->remembered_set)
            return -1;
        if (chunk_meta->iter_visited_end)
            return -2;
        mem_chunk_foreach_allocated_object(
            chunk, sizeof(struct old_space_chunk_meta), obj, obj_class, obj_size,
        {
            (ow_unused_var(obj_class), ow_unused_var(obj_size));
            if (!(ow_object_meta_test_(OLD, obj->_meta) && !ow_object_meta_test_(BIG, obj->_meta)))
                return -8;
            if (ow_object_meta_test_(MRK, obj->_meta))
                return -9;
            if (old_space_chunk_meta_of_obj(obj) != chunk_meta)
                return -10;
        });
    });
    return 0;
}

#endif // OW_DEBUG_MEMORY

/* ----- New space (young generation) --------------------------------------- */

/*
 * In new space, mark-copy GC algorithm is used.
 * The PTR field in object meta is not used.
 */

/// New space manager.
struct new_space {
    struct mem_chunk *_working_chunk, *_free_chunk;
};

/// Initialize space.
static void new_space_init(struct new_space *space) {
    const size_t chunk_size = NEW_SPACE_CHUNK_SIZE;
    space->_working_chunk = mem_chunk_create(chunk_size);
    space->_free_chunk    = mem_chunk_create(chunk_size);
}

/// Finalize allocated objects and the space.
static void new_space_fini(struct new_space *space) {
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        ow_unused_var(obj_size);
        call_object_finalizer(obj, obj_class);
    });
    mem_chunk_destroy(space->_working_chunk);
    mem_chunk_destroy(space->_free_chunk);
}

#if OW_DEBUG_MEMORY

static void new_space_print_usage(struct new_space *space, FILE *stream) {
    fputs("<NewSpc>\n", stream);
    struct mem_chunk *const chunks[2] = {space->_working_chunk, space->_free_chunk};
    for (int i = 0; i < 2; i++) {
        struct mem_chunk *const chunk = chunks[i];
        fprintf(
            stream,
            "  <chunk addr=\"%p\" is_working_chunk=\"%s\" "
            "size=\"%zu\" free_size=\"%zu\" />\n",
            (void *)chunk,
            i == 0 ? "yes" : "no",
            (size_t)(chunk->_end - chunk->_mem),
            (size_t)(chunk->_end - chunk->_free)
        );
    }
    fputs("</NewSpc>\n", stream);
}

#endif // OW_DEBUG_MEMORY

/// Allocate storage for an object. On failure, returns `NULL`.
ow_forceinline static struct ow_object *
new_space_alloc(struct new_space *space, void *class_, size_t size) {
    assert(size >= sizeof(struct ow_object_meta));
    struct ow_object *const obj =
        mem_chunk_alloc(space->_working_chunk, size);
    if (ow_unlikely(!obj))
        return NULL;
    assert(ow_object_meta_check_value(class_));
    ow_object_meta_init(obj->_meta, false, false, 0U, class_);
    return obj;
}

/// Fast GC: reallocate and copy objects that are marked alive in new space.
/// For objects that survived only once, new storages are in the other chunk,
/// which are still in new space. But the `MID` flag in object meta is set.
/// For other (older) objects, new storages are allocated in old space.
/// If the old space fails to allocate storage, they are kept in new space,
/// and `false` will be returned at the end of function.
/// New storage address is written to the `PTR` field of object meta.
/// Dead objects are finalized.
static bool new_space_realloc_and_copy_survivors(
    struct new_space *space, struct old_space *old_space
) {
    struct mem_chunk *const to_chunk = space->_free_chunk;
    mem_chunk_forget(to_chunk);

    bool old_space_is_full = false;
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        assert(!ow_object_meta_test_(OLD, obj->_meta));
        if (ow_likely(!ow_object_meta_test_(MRK, obj->_meta))) {
            call_object_finalizer(obj, obj_class);
            continue;
        }

        struct ow_object *new_obj;
        if (!ow_object_meta_test_(MID, obj->_meta)) {
        alloc_in_new_space:
            new_obj = mem_chunk_alloc(to_chunk, obj_size);
            assert(new_obj);
            ow_object_meta_init(new_obj->_meta, false, true, 0U, obj_class);
        } else {
            if (ow_unlikely(old_space_is_full))
                goto alloc_in_new_space;
            new_obj = old_space_alloc(old_space, obj_class, obj_size);
            if (ow_unlikely(!new_obj)) {
                old_space_is_full = true;
                goto alloc_in_new_space;
            }
        }

        ow_object_meta_store_(PTR, obj->_meta, new_obj);
        assert((char *)new_obj < (char *)obj || (char *)new_obj >= (char *)obj + obj_size);
        memcpy(
            (char *)new_obj + OW_OBJECT_HEAD_SIZE,
            (char *)obj + OW_OBJECT_HEAD_SIZE,
            obj_size - OW_OBJECT_HEAD_SIZE
        );
    });

    return !old_space_is_full;
}

/// Full GC: reallocate storages for survivors. Objects are neither initialized
/// nor moved. Pointer to new storage is written to the `PTR` field of object meta.
/// The rules are same with that in function `new_space_realloc_and_copy_survivors()`.
static void new_space_realloc_survivors(
    struct new_space *space,
    struct old_space *old_space, struct old_space_iterator *old_space_realloc_iter
) {
    struct mem_chunk *const to_chunk = space->_free_chunk;
    mem_chunk_forget(to_chunk);

    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        ow_unused_var(obj_class);
        assert(!ow_object_meta_test_(OLD, obj->_meta));
        if (ow_likely(!ow_object_meta_test_(MRK, obj->_meta))) {
            call_object_finalizer(obj, obj_class);
            continue;
        }

        void *new_mem;
        if (!ow_object_meta_test_(MID, obj->_meta)) {
            new_mem = mem_chunk_alloc(to_chunk, obj_size);
            assert(new_mem);
        } else {
            new_mem = old_space_fake_alloc(
                old_space, old_space_realloc_iter, obj_size
            );
        }

        assert(ow_object_meta_check_value(new_mem));
        ow_object_meta_store_(PTR, obj->_meta, new_mem);
    });
}

/// GC: swap two chunks.
static void new_space_swap_chunks(struct new_space *space) {
    struct mem_chunk *tmp = space->_free_chunk;
    space->_free_chunk    = space->_working_chunk;
    space->_working_chunk = tmp;
}

/// Fast GC: update references to the moved objects that are still in new space.
/// Only references in the `working_chunk` are updated.
/// DO NOT forget to swap chunks before calling this function!
static void new_space_update_references(struct new_space *space) {
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        (ow_unused_var(obj_class), ow_unused_var(obj_size));
        _ow_objmem_move_object_fields(obj);
    });
}

/// Full GC: update references like `new_space_update_references()`, but only
/// references in objects that are not marked will be skipped.
static void new_space_update_marked_references(struct new_space *space) {
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        (ow_unused_var(obj_class), ow_unused_var(obj_size));
        if (ow_likely(!ow_object_meta_test_(MRK, obj->_meta)))
            continue;
        _ow_objmem_move_object_fields(obj);
    });
}

/// Full GC: move survived objects in `working_chunk` to new storage.
static void new_space_move_marked_objects(
    struct new_space *space,
    struct old_space_init_reallocated_obj_meta_context *ctx
) {
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        if (ow_likely(!ow_object_meta_test_(MRK, obj->_meta)))
            continue;

        ow_object_meta_reset_(MRK, obj->_meta);

        struct ow_object *const new_obj =
            ow_object_meta_load_(PTR, struct ow_object *, obj->_meta);

        if (!ow_object_meta_test_(MID, obj->_meta)) {
            ow_object_meta_init(new_obj->_meta, false, true, 0, obj_class);
        } else {
            old_space_init_reallocated_obj_meta(ctx, new_obj, obj_class);
        }

        assert((char *)new_obj < (char *)obj || (char *)new_obj >= (char *)obj + obj_size);
        memcpy(
            (char *)new_obj + OW_OBJECT_HEAD_SIZE,
            (char *)obj + OW_OBJECT_HEAD_SIZE,
            obj_size - OW_OBJECT_HEAD_SIZE
        );
    });
}

#if OW_DEBUG_MEMORY

static int new_space_post_gc_check(struct new_space *space) {
    mem_chunk_foreach_allocated_object(
        space->_working_chunk, 0, obj, obj_class, obj_size,
    {
        (ow_unused_var(obj_class), ow_unused_var(obj_size));
        if (ow_object_meta_test_(OLD, obj->_meta))
            return -1;
        if (ow_object_meta_test_(MRK, obj->_meta))
            return -2;
    });
    return 0;
}

#endif // OW_DEBUG_MEMORY

/* ----- Public functions --------------------------------------------------- */

struct ow_objmem_context {
    size_t no_gc_count;   ///< If greater than 0, do not run GC.
    bool   force_full_gc; ///< GC type must be full GC.
    int8_t current_gc_type;

    struct new_space new_space;
    struct old_space old_space;
    struct big_space big_space;

    struct mem_span_set gc_roots;
    struct mem_span_set weak_refs;

#if OW_DEBUG_MEMORY
    bool verbose;
#endif // OW_DEBUG_MEMORY
};

static_assert(
    offsetof(struct ow_machine, objmem_context) == 0 &&
    offsetof(struct ow_objmem_context, no_gc_count) == 0,
    "_ow_objmem_ngc_count()");

struct ow_objmem_context *ow_objmem_context_new(void) {
    struct ow_objmem_context *const ctx = ow_malloc(sizeof(struct ow_objmem_context));
    ctx->no_gc_count = 0;
    ctx->force_full_gc = false;
    ctx->current_gc_type = (int8_t)OW_OBJMEM_GC_NONE;
    new_space_init(&ctx->new_space);
    old_space_init(&ctx->old_space);
    big_space_init(&ctx->big_space);
    mem_span_set_init(&ctx->gc_roots);
    mem_span_set_init(&ctx->weak_refs);
#if OW_DEBUG_MEMORY
    ctx->verbose = false;
#endif // OW_DEBUG_MEMORY
    return ctx;
}

void ow_objmem_context_del(struct ow_objmem_context *ctx) {
    mem_span_set_fini(&ctx->weak_refs);
    mem_span_set_fini(&ctx->gc_roots);

    big_space_fini(&ctx->big_space);
    new_space_fini(&ctx->new_space);
    old_space_pre_fini(&ctx->old_space);
    old_space_fini(&ctx->old_space);
    /*
     * Classes are allocated in old space. Free storages in old space last
     * so that the classes are accessible when finalizing all objects.
     */

    ow_free(ctx);
}

void ow_objmem_context_verbose(struct ow_objmem_context *ctx, bool status) {
    ow_unused_var(ctx);
    ow_unused_var(status);
#if OW_DEBUG_PARSER
    ctx->verbose = status;
#endif // OW_DEBUG_PARSER
}

struct ow_object *ow_objmem_allocate(
    struct ow_machine *om, struct ow_class_obj *obj_class
) {
    struct ow_object *obj;
    struct ow_objmem_context *const ctx = om->objmem_context;

    assert(!ow_class_obj_pub_info(obj_class)->has_extra_fields);
    const size_t obj_size =
        OW_OBJECT_SIZE(ow_class_obj_pub_info(obj_class)->basic_field_count);

    if (ow_likely(obj_size <= NON_BIG_SPACE_MAX_ALLOC_SIZE)) {
    alloc_small:
        obj = new_space_alloc(&ctx->new_space, obj_class, obj_size);
        if (ow_unlikely(!obj)) {
            ow_objmem_gc(om, OW_OBJMEM_GC_FAST);
            goto alloc_small;
        }
    } else {
    alloc_large:
        obj = big_space_alloc(&ctx->big_space, obj_class, obj_size);
        if (ow_unlikely(!obj)) {
            ow_objmem_gc(om, OW_OBJMEM_GC_FULL);
            goto alloc_large;
        }
    }

    assert(!ow_smallint_check(obj));
    assert(ow_object_class(obj) == obj_class);
    return obj;
}

struct ow_object *ow_objmem_allocate_ex(
    struct ow_machine *om, enum ow_objmem_alloc_type alloc_type,
    struct ow_class_obj *obj_class, size_t extra_field_count
) {
    struct ow_object *obj;
    struct ow_objmem_context *const ctx = om->objmem_context;
    const struct ow_class_obj_pub_info *const obj_class_info =
        ow_class_obj_pub_info(obj_class);
    const bool has_extra_fields = obj_class_info->has_extra_fields;
    assert(has_extra_fields || !extra_field_count);
    assert(!has_extra_fields || obj_class_info->basic_field_count >= 1);
    const size_t obj_field_count =
        obj_class_info->basic_field_count + extra_field_count;
    const size_t obj_size = OW_OBJECT_SIZE(obj_field_count);

    if (ow_likely(alloc_type == OW_OBJMEM_ALLOC_AUTO)) {
    alloc_type_auto:
        if (ow_unlikely(obj_size > NON_BIG_SPACE_MAX_ALLOC_SIZE))
            goto alloc_type_huge;
    alloc_small:
        obj = new_space_alloc(&ctx->new_space, obj_class, obj_size);
        if (ow_unlikely(!obj)) {
            ow_objmem_gc(om, OW_OBJMEM_GC_FAST);
            goto alloc_small;
        }
    } else if (ow_likely(alloc_type == OW_OBJMEM_ALLOC_SURV)) {
        if (ow_unlikely(obj_size > NON_BIG_SPACE_MAX_ALLOC_SIZE))
            goto alloc_type_huge;
    alloc_type_surv:
        obj = old_space_alloc(&ctx->old_space, obj_class, obj_size);
        if (ow_unlikely(!obj)) {
            ow_objmem_gc(om, OW_OBJMEM_GC_FULL);
            goto alloc_type_surv;
        }
    } else if (ow_likely(alloc_type == OW_OBJMEM_ALLOC_HUGE)) {
    alloc_type_huge:;
        int retrying = 0;
    alloc_huge_retry:
        obj = big_space_alloc(&ctx->big_space, obj_class, obj_size);
        if (ow_unlikely(!obj)) {
            assert(retrying == 0 || retrying == 1);
            if (retrying)
                ctx->big_space.threshold_size =
                    ctx->big_space.allocated_size + obj_size; // TODO: check heap limit.
            else
                ow_objmem_gc(om, OW_OBJMEM_GC_FULL);
            retrying++;
            goto alloc_huge_retry;
        }
    } else {
        goto alloc_type_auto;
    }

    if (has_extra_fields) {
        struct _fake_extended_object { OW_EXTENDED_OBJECT_HEAD };
        assert(obj_field_count >= 1 && obj_field_count <= OW_SMALLINT_MAX);
        ((struct _fake_extended_object *)obj)->field_count = obj_field_count;
    }

    assert(!ow_smallint_check(obj));
    assert(ow_object_class(obj) == obj_class);
    assert(ow_class_obj_object_field_count(obj_class, obj) == obj_field_count);
    return obj;
}

void ow_objmem_add_gc_root(
    struct ow_machine *om,
    void *root, ow_objmem_obj_fields_visitor_t fn
) {
    struct ow_objmem_context *const ctx = om->objmem_context;
    mem_span_set_add(&ctx->gc_roots, root, (void(*)(void))fn);
}

bool ow_objmem_remove_gc_root(struct ow_machine *om, void *root) {
    struct ow_objmem_context *const ctx = om->objmem_context;
    return mem_span_set_remove(&ctx->gc_roots, root);
}

void ow_objmem_register_weak_ref(
    struct ow_machine *om,
    void *ref_container, ow_objmem_weak_refs_visitor_t fn
) {
    struct ow_objmem_context *const ctx = om->objmem_context;
    mem_span_set_add(&ctx->weak_refs, ref_container, (void(*)(void))fn);
}

bool ow_objmem_remove_weak_ref(struct ow_machine *om, void *ref_container) {
    struct ow_objmem_context *const ctx = om->objmem_context;
    return mem_span_set_remove(&ctx->weak_refs, ref_container);
}

/// Fast (young) GC implementation.
static void gc_fast(struct ow_objmem_context *ctx) {
    // ## 1  Mark reachable young objects.

    // ### 1.1  Mark young objects in GC roots.

    mem_span_set_foreach(
        &ctx->gc_roots,
        void *, gc_root,
        ow_objmem_obj_fields_visitor_t, fields_visitor,
    {
        fields_visitor(gc_root, OW_OBJMEM_OBJ_VISIT_MARK_REC_Y);
    });

    // ### 1.2  Scan remembered sets and mark referred young objects.

    const size_t old_spc_cnt_hint =
        old_space_mark_remembered_objects_young_fields(&ctx->old_space);

    // ### 1.3  Scan big space and mark referred young objects.

    const size_t big_spc_cnt_hint =
        big_space_mark_remembered_objects_young_fields(&ctx->big_space);

    // ## 2  Clean up unused weak references.

    mem_span_set_foreach(
        &ctx->weak_refs,
        void *, weak_ref,
        ow_objmem_weak_refs_visitor_t, visitor,
    {
        visitor(weak_ref, OW_OBJMEM_WEAK_REF_VISIT_FINI_Y);
    });

    // ## 3  Re-allocate storage for survived objects, then copy them to new places.

    const struct old_space_iterator old_spc_orig_end =
        old_space_allocated_end(&ctx->old_space);

    if (!new_space_realloc_and_copy_survivors(&ctx->new_space, &ctx->old_space))
        ctx->force_full_gc = true; // Run full GC next time.

    /* `_ow_objmem_mark_old_referred_object_young_fields()` is used when marking
     * remembered young objects in old space and big space. These marked young
     * objects referred by old ones shall be moved to old space.
     *
     *  Dead objects are finalized.
     */

    // ## 4  Update references.

    // ### 4.1  Update references in newly allocated objects in new space.

    new_space_swap_chunks(&ctx->new_space);
    new_space_update_references(&ctx->new_space);

    // ### 4.2  Update references in newly allocated objects in old space.

    old_space_update_references_from(&ctx->old_space, old_spc_orig_end);

    // ### 4.3  Update references in remembered old objects.

    old_space_update_remembered_objects_references_forget_remembered_objects(&ctx->old_space, old_spc_cnt_hint);

    // ### 4.4  Update references in remembered large objects.

    big_space_update_remembered_objects_references_and_forget_remembered_objects(&ctx->big_space, big_spc_cnt_hint);

    // ### 4.5  Update references in GC roots.

    mem_span_set_foreach(
        &ctx->gc_roots,
        void *, gc_root,
        ow_objmem_obj_fields_visitor_t, fields_visitor,
    {
        fields_visitor(gc_root, OW_OBJMEM_OBJ_VISIT_MOVE);
    });

    //### 4.6  Update references in weak references.

    mem_span_set_foreach(
        &ctx->weak_refs,
        void *, weak_ref,
        ow_objmem_weak_refs_visitor_t, visitor,
    {
        visitor(weak_ref, OW_OBJMEM_WEAK_REF_VISIT_MOVE);
    });
}

/// Full (young + old) GC implementation.
static void gc_full(struct ow_objmem_context *ctx) {
    // ## 1  Mark reachable objects in GC roots.

    mem_span_set_foreach(
        &ctx->gc_roots,
        void *, gc_root,
        ow_objmem_obj_fields_visitor_t, fields_visitor,
    {
        fields_visitor(gc_root, OW_OBJMEM_OBJ_VISIT_MARK_REC);
    });

    // ## 2  Clean up unused weak references.

    mem_span_set_foreach(
        &ctx->weak_refs,
        void *, weak_ref,
        ow_objmem_weak_refs_visitor_t, visitor,
    {
        visitor(weak_ref, OW_OBJMEM_WEAK_REF_VISIT_FINI);
    });

    // ## 3  Re-allocate storage for survived objects. Remove dead ones.

    // ### 3.1  Finalize and delete unreachable objects in big space. No re-allocation.

    big_space_delete_unreachable_objects_and_reset_reachable_objects(&ctx->big_space);

    // ### 3.2  Re-allocations in old space. Finalize dead ones.

    struct old_space_iterator old_spc_realloc_iter = old_space_allocated_begin(&ctx->old_space);
    old_space_realloc_survivors_and_forget_remembered_objects(&ctx->old_space, &old_spc_realloc_iter);

    // ### 3.3  Re-allocations in new space. Finalize dead ones.

    new_space_realloc_survivors(&ctx->new_space, &ctx->old_space, &old_spc_realloc_iter);

    // ## 4  Update references.

    // ### 4.1  Update references in new space.

    new_space_update_marked_references(&ctx->new_space);

    // ### 4.2  Update references in old space.

    old_space_update_references(&ctx->old_space);

    // ### 4.3  Update references in big space.

    big_space_update_references(&ctx->big_space);

    // ### 4.5  Update references in GC roots.

    mem_span_set_foreach(
        &ctx->gc_roots,
        void *, gc_root,
        ow_objmem_obj_fields_visitor_t, fields_visitor,
    {
        fields_visitor(gc_root, OW_OBJMEM_OBJ_VISIT_MOVE);
    });

    //### 4.6  Update references in weak references.

    mem_span_set_foreach(
        &ctx->weak_refs,
        void *, weak_ref,
        ow_objmem_weak_refs_visitor_t, visitor,
    {
        visitor(weak_ref, OW_OBJMEM_WEAK_REF_VISIT_MOVE);
    });

    // ### 5  Move objects to new storage.

    // ### 5.1  Move objects in old space.

    struct old_space_init_reallocated_obj_meta_context old_spc_init_obj_ctx =
        old_space_init_reallocated_obj_meta_context(&ctx->old_space);
    old_space_move_reallocated_objects(&ctx->old_space, &old_spc_init_obj_ctx);

    // ### 5.2  Move objects in new space.

    new_space_move_marked_objects(&ctx->new_space, &old_spc_init_obj_ctx);
    new_space_swap_chunks(&ctx->new_space);

    // ### 5.3  Clean up unused old space chunks.

    old_space_truncate(&ctx->old_space, old_spc_realloc_iter);

    // TODO: adjust big space threshold.
}

int ow_objmem_gc(struct ow_machine *om, enum ow_objmem_gc_type type) {
    struct ow_objmem_context *const ctx = om->objmem_context;

    if (ow_unlikely(ctx->no_gc_count))
        return -1;

    if (ow_unlikely(ctx->force_full_gc)) {
        ctx->force_full_gc = false;
        type = OW_OBJMEM_GC_FULL;
    } else if (type == OW_OBJMEM_GC_AUTO) {
        type = OW_OBJMEM_GC_FAST;
    }
    ctx->current_gc_type = (int8_t)type;

#if OW_DEBUG_MEMORY
    if (ow_unlikely(ctx->verbose)) {
        const char *const type_name = type == OW_OBJMEM_GC_FAST ? "fast" : "full";
        fprintf(stderr, "[GC] %s GC starts\n", type_name);
    }

    struct timespec ts0;
    timespec_get(&ts0, TIME_UTC);
#endif // OW_DEBUG_MEMORY

    if (type == OW_OBJMEM_GC_FAST)
        gc_fast(ctx);
    else if (type == OW_OBJMEM_GC_FULL)
        gc_full(ctx);
    else
        type = OW_OBJMEM_GC_NONE; // Illegal type.

#if OW_DEBUG_MEMORY
    if (ow_unlikely(ctx->verbose)) {
        struct timespec ts1;
        timespec_get(&ts1, TIME_UTC);
        double dt_ms =
            (double)(ts1.tv_sec - ts0.tv_sec) * 1e3 +
            (double)(ts1.tv_nsec - ts0.tv_nsec) / 1e6;

        fprintf(stderr, "[GC] GC ends, %.1lf ms\n", dt_ms);

        fputs("[GC] ow_objmem_print_usage() vvv\n", stderr);
        ow_objmem_print_usage(ctx, NULL);
        fputs("[GC] ow_objmem_print_usage() ^^^\n", stderr);
    }

    assert(!new_space_post_gc_check(&ctx->new_space));
    assert(!old_space_post_gc_check(&ctx->old_space));
    assert(!big_space_post_gc_check(&ctx->big_space));
#endif // OW_DEBUG_MEMORY

    ctx->current_gc_type = (int8_t)OW_OBJMEM_GC_NONE;

    return (int)type;
}

enum ow_objmem_gc_type ow_objmem_current_gc(struct ow_machine *om) {
    struct ow_objmem_context *const ctx = om->objmem_context;
    return (enum ow_objmem_gc_type)(int)ctx->current_gc_type;
}

ow_noinline void ow_objmem_record_o2y_object(struct ow_object *obj) {
    assert(ow_object_meta_test_(OLD, obj->_meta));
    if (ow_likely(!ow_object_meta_test_(BIG, obj->_meta)))
        old_space_add_remembered_object(old_space_chunk_meta_of_obj(obj), obj);
    else
        big_space_remember_object(obj);
}

void ow_objmem_print_usage(struct ow_objmem_context *ctx, void *FILE_ptr) {
#if OW_DEBUG_MEMORY

    FILE *stream = FILE_ptr ? FILE_ptr : stderr;

    fprintf(
        stream, "<ObjMem context=\"%p\" force_full_gc=\"%s\">\n",
        (void *)ctx, ctx->force_full_gc ? "yes" : "no"
    );
    new_space_print_usage(&ctx->new_space, stream);
    old_space_print_usage(&ctx->old_space, stream);
    big_space_print_usage(&ctx->big_space, stream);
    fputs("</ObjMem>\n", stream);

#else // !OW_DEBUG_MEMORY

    ow_unused_var(ctx);
    ow_unused_var(FILE_ptr);

    // Not available!

#endif
}

ow_noinline void _ow_objmem_mark_object_fields_rec(struct ow_object *obj) {
    struct ow_class_obj *const obj_class = ow_object_class(obj);
    const struct ow_class_obj_pub_info *const info =
        ow_class_obj_pub_info(obj_class);

    _ow_objmem_visit_object_do_mark(
        ow_object_from(obj_class),
        OW_OBJMEM_OBJ_VISIT_MARK_REC
    );

    size_t field_index;
    if (ow_unlikely(info->native_field_count)) {
        const ow_objmem_obj_fields_visitor_t fields_visitor = info->gc_visitor;
        if (fields_visitor)
            fields_visitor(obj, OW_OBJMEM_OBJ_VISIT_MARK_REC);
        if (info->has_extra_fields)
            return; // Extra fields shall have been visited by `fields_visitor`.
        field_index = info->native_field_count;
    } else {
        assert(!info->gc_visitor);
        assert(!info->has_extra_fields);
        field_index = 0;
    }

    const size_t field_count = info->basic_field_count;
    assert(field_index <= field_count);
    for (; field_index < field_count; field_index++) {
        ow_objmem_visit_object(
            obj->_fields[field_index],
            OW_OBJMEM_OBJ_VISIT_MARK_REC
        );
    }
}

ow_noinline void _ow_objmem_mark_object_young_fields_rec(struct ow_object *obj) {
    // Modified from `_ow_objmem_mark_object_fields_rec()`.

    struct ow_class_obj *const obj_class = ow_object_class(obj);
    const struct ow_class_obj_pub_info *const info =
        ow_class_obj_pub_info(obj_class);

    assert(ow_object_meta_test_(OLD, ow_object_from(obj_class)->_meta));

    size_t field_index;
    if (ow_unlikely(info->native_field_count)) {
        const ow_objmem_obj_fields_visitor_t fields_visitor = info->gc_visitor;
        if (fields_visitor)
            fields_visitor(obj, OW_OBJMEM_OBJ_VISIT_MARK_REC_Y);
        if (info->has_extra_fields)
            return;
        field_index = info->native_field_count;
    } else {
        assert(!info->gc_visitor);
        assert(!info->has_extra_fields);
        field_index = 0;
    }

    const size_t field_count = info->basic_field_count;
    assert(field_index <= field_count);
    for (; field_index < field_count; field_index++) {
        ow_objmem_visit_object(
            obj->_fields[field_index],
            OW_OBJMEM_OBJ_VISIT_MARK_REC_Y
        );
    }
}

void _ow_objmem_mark_old_referred_object_fields_rec(struct ow_object *obj) {
    // Modified from `_ow_objmem_mark_object_fields_rec()`.

    struct ow_class_obj *const obj_class = ow_object_class(obj);
    const struct ow_class_obj_pub_info *const info =
        ow_class_obj_pub_info(obj_class);

    assert(ow_object_meta_test_(OLD, ow_object_from(obj_class)->_meta));

    size_t field_index;
    if (ow_unlikely(info->native_field_count)) {
        const ow_objmem_obj_fields_visitor_t fields_visitor = info->gc_visitor;
        if (fields_visitor)
            fields_visitor(obj, OW_OBJMEM_OBJ_VISIT_MARK_REC_O2X);
        if (info->has_extra_fields)
            return;
        field_index = info->native_field_count;
    } else {
        assert(!info->gc_visitor);
        assert(!info->has_extra_fields);
        field_index = 0;
    }

    const size_t field_count = info->basic_field_count;
    assert(field_index <= field_count);
    for (; field_index < field_count; field_index++) {
        ow_objmem_visit_object(
            obj->_fields[field_index],
            OW_OBJMEM_OBJ_VISIT_MARK_REC_O2X
        );
    }
}

ow_noinline void _ow_objmem_mark_old_referred_object_young_fields_rec(struct ow_object *obj) {
    // Modified from `_ow_objmem_mark_object_fields_rec()`.

    struct ow_class_obj *const obj_class = ow_object_class(obj);
    const struct ow_class_obj_pub_info *const info =
        ow_class_obj_pub_info(obj_class);

    assert(ow_object_meta_test_(OLD, ow_object_from(obj_class)->_meta));

    size_t field_index;
    if (ow_unlikely(info->native_field_count)) {
        const ow_objmem_obj_fields_visitor_t fields_visitor = info->gc_visitor;
        if (fields_visitor)
            fields_visitor(obj, OW_OBJMEM_OBJ_VISIT_MARK_REC_O2Y);
        if (info->has_extra_fields)
            return;
        field_index = info->native_field_count;
    } else {
        assert(!info->gc_visitor);
        assert(!info->has_extra_fields);
        field_index = 0;
    }

    const size_t field_count = info->basic_field_count;
    assert(field_index <= field_count);
    for (; field_index < field_count; field_index++) {
        ow_objmem_visit_object(
            obj->_fields[field_index],
            OW_OBJMEM_OBJ_VISIT_MARK_REC_O2Y
        );
    }
}

ow_noinline void _ow_objmem_move_object_fields(struct ow_object *obj) {
    // Modified from `_ow_objmem_mark_object_fields_rec()`.

    struct ow_class_obj *obj_class = ow_object_class(obj);
    const struct ow_class_obj_pub_info *const info =
        ow_class_obj_pub_info(obj_class); // DO NOT get info after class ptr updated.

    if (ow_unlikely(_ow_objmem_visit_object_do_move((struct ow_object **)&obj_class)))
        ow_object_meta_store_(CLS, obj->_meta, obj_class);

    size_t field_index;
    if (ow_unlikely(info->native_field_count)) {
        const ow_objmem_obj_fields_visitor_t fields_visitor = info->gc_visitor;
        if (fields_visitor)
            fields_visitor(obj, OW_OBJMEM_OBJ_VISIT_MOVE);
        if (info->has_extra_fields)
            return;
        field_index = info->native_field_count;
    } else {
        assert(!info->gc_visitor);
        assert(!info->has_extra_fields);
        field_index = 0;
    }

    const size_t field_count = info->basic_field_count;
    assert(field_index <= field_count);
    for (; field_index < field_count; field_index++) {
        ow_objmem_visit_object(
            obj->_fields[field_index],
            OW_OBJMEM_OBJ_VISIT_MOVE
        );
    }
}
