#pragma once

#define _ow_pragma_string_(X) #X
#define ow_pragma_string(X) _ow_pragma_string_(X)

#if defined(__GNUC__) || defined(_MSC_VER)
#    define ow_pragma_message(MSG) _Pragma(ow_pragma_string(message(MSG)))
#else
#    define ow_pragma_message(MSG)
#endif
