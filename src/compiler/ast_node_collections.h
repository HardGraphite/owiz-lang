// For "ast.h" only.

#include <utilities/array.h>
#include <utilities/attributes.h>

// An array of AST nodes.
struct ow_ast_node_array {
	struct ow_array _data;
};

// Initialize the array.
ow_static_inline void ow_ast_node_array_init(struct ow_ast_node_array *arr) {
	ow_array_init(&arr->_data, 8);
}

// Delete all nodes.
ow_static_inline void ow_ast_node_array_clear(struct ow_ast_node_array *arr) {
	struct ow_ast_node **nodes =
			(struct ow_ast_node **)ow_array_data(&arr->_data);
	for (size_t i = 0, n = ow_array_size(&arr->_data); i < n; i++)
		ow_ast_node_del(nodes[i]);
	ow_array_clear(&arr->_data);
}

// Finalize the array.
ow_static_inline void ow_ast_node_array_fini(struct ow_ast_node_array *arr) {
	ow_ast_node_array_clear(arr);
	ow_array_fini(&arr->_data);
}

// Get number of nodes.
ow_static_inline size_t ow_ast_node_array_size(const struct ow_ast_node_array *arr) {
	return ow_array_size(&arr->_data);
}

// Get node by index.
ow_static_inline struct ow_ast_node *ow_ast_node_array_at(
		struct ow_ast_node_array *arr, size_t index) {
	return ow_array_at(&arr->_data, index);
}

// View last node.
ow_static_inline struct ow_ast_node *ow_ast_node_array_last(struct ow_ast_node_array *arr) {
	return ow_array_at(&arr->_data, ow_array_size(&arr->_data) - 1);
}

// Append a node.
ow_static_inline void ow_ast_node_array_append(
		struct ow_ast_node_array *arr, struct ow_ast_node *node) {
	ow_array_append(&arr->_data, node);
}

// Drop last node without delete it.
ow_static_inline struct ow_ast_node *ow_ast_node_array_drop(struct ow_ast_node_array *arr) {
	struct ow_ast_node *const node = ow_ast_node_array_last(arr);
	ow_array_drop(&arr->_data);
	return node;
}

struct ow_ast_nodepair_array_elem {
	struct ow_ast_node *first, *second;
};

// An array of AST node pairs.
struct ow_ast_nodepair_array {
	struct ow_xarray _data;
};

// Initialize the array.
ow_static_inline void ow_ast_nodepair_array_init(struct ow_ast_nodepair_array *arr) {
	ow_xarray_init(&arr->_data, struct ow_ast_nodepair_array_elem, 8);
}

// Delete all nodes.
ow_static_inline void ow_ast_nodepair_array_clear(struct ow_ast_nodepair_array *arr) {
	struct ow_ast_nodepair_array_elem *nodes =
		ow_xarray_data(&arr->_data, struct ow_ast_nodepair_array_elem);
	for (size_t i = 0, n = ow_xarray_size(&arr->_data); i < n; i++)
		ow_ast_node_del(nodes[i].first), ow_ast_node_del(nodes[i].second);
	ow_xarray_clear(&arr->_data);
}

// Finalize the array.
ow_static_inline void ow_ast_nodepair_array_fini(struct ow_ast_nodepair_array *arr) {
	ow_ast_nodepair_array_clear(arr);
	ow_xarray_fini(&arr->_data);
}

// Get number of nodes.
ow_static_inline size_t ow_ast_nodepair_array_size(
		const struct ow_ast_nodepair_array *arr) {
	return ow_xarray_size(&arr->_data);
}

// Get node by index.
ow_static_inline struct ow_ast_nodepair_array_elem ow_ast_nodepair_array_at(
		struct ow_ast_nodepair_array *arr, size_t index) {
	return ow_xarray_at(&arr->_data, struct ow_ast_nodepair_array_elem, index);
}

// View last node.
ow_static_inline struct ow_ast_nodepair_array_elem ow_ast_nodepair_array_last(
		struct ow_ast_nodepair_array *arr) {
	const size_t index = ow_xarray_size(&arr->_data) - 1;
	return ow_xarray_at(&arr->_data, struct ow_ast_nodepair_array_elem, index);
}

// Append a node.
ow_static_inline void ow_ast_nodepair_array_append(
		struct ow_ast_nodepair_array *arr, struct ow_ast_nodepair_array_elem node) {
	ow_xarray_append(&arr->_data, struct ow_ast_nodepair_array_elem, node);
}

// Drop last node without delete it.
ow_static_inline struct ow_ast_nodepair_array_elem ow_ast_nodepair_array_drop(
		struct ow_ast_nodepair_array *arr) {
	const struct ow_ast_nodepair_array_elem node = ow_ast_nodepair_array_last(arr);
	ow_xarray_drop(&arr->_data);
	return node;
}
