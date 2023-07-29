#include <type_traits>

#ifndef __cplusplus
#   error "`__cplusplus` is not defined"
#endif // __cplusplus

#include <owiz.h>

int main() {
    static_assert(std::is_nothrow_invocable_r_v<owiz_sysconf_result, decltype(owiz_sysconf), int>);
    owiz_sysconf(0);
}
