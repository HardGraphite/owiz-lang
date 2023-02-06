#include <type_traits>

#ifndef __cplusplus
#	error "`__cplusplus` is not defined"
#endif // __cplusplus

#include <owiz.h>

int main() {
	static_assert(std::is_nothrow_invocable_r_v<ow_sysconf_result, decltype(ow_sysconf), int>);
	ow_sysconf(0);
}
