#pragma once

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG >= 201703L, "SISTEMAS-MMO requires C++17 or newer");
#else
static_assert(__cplusplus >= 201703L, "SISTEMAS-MMO requires C++17 or newer");
#endif
