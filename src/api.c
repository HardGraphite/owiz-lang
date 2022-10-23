#define OW_EXPORT_API 1
#include <ow.h>

#include <assert.h>

#include <machine/machine.h>

OW_API ow_machine_t *ow_create(void) {
	return ow_machine_new();
}

OW_API void ow_destroy(ow_machine_t *om) {
	assert(om);
	ow_machine_del(om);
}
