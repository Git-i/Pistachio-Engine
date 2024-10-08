#pragma once
#include <memory>
#ifdef _MSC_VER
#define PT_DEBUG_BREAK __debugbreak()
#else
#include <signal.h>
#define PT_DEBUG_BREAK raise(SIGTRAP);
#endif

typedef int KeyCode;
#ifdef PT_USE_MENUM
#include "magic_enum.hpp"
#define ENUM_FMT(val) magic_enum::enum_name(val)
#else
#define ENUM_FMT(val) static_cast<std::underlying_type_t<decltype(val)>>(val)
#endif
#define ENUM_FLAGS(EnumType)                      \
inline constexpr EnumType operator|(EnumType a, EnumType b) {                             \
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(a) | static_cast<std::underlying_type_t<EnumType>>(b));    \
}                                                                               \
inline constexpr EnumType operator&(EnumType a, EnumType b) {                             \
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(a) & static_cast<std::underlying_type_t<EnumType>>(b));    \
}                                                                               \
inline constexpr EnumType operator^(EnumType a, EnumType b) {                             \
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(a) ^ static_cast<std::underlying_type_t<EnumType>>(b));    \
}                                                                               \
inline constexpr EnumType operator~(EnumType a) {                                         \
    return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(a));                         \
}                                                                               \
inline constexpr EnumType& operator|=(EnumType& a, EnumType b) {                         \
    a = a | b;                                                                 \
    return a;                                                                  \
}                                                                               \
inline constexpr EnumType& operator&=(EnumType& a, EnumType b) {                         \
    a = a & b;                                                                 \
    return a;                                                                  \
}                                                                               \
inline constexpr EnumType& operator^=(EnumType& a, EnumType b) {                         \
    a = a ^ b;                                                                 \
    return a;                                                                  \
}


#ifdef PT_PLATFORM_WINDOWS
#ifdef DYNAMICLINK
	#ifdef PISTACHIO_BUILD_DLL
		#define PISTACHIO_API __declspec(dllexport)
	#else
		#define PISTACHIO_API __declspec(dllimport)
	#endif
#else
	#define PISTACHIO_API
#endif
#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "Xinput9_1_0.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma warning( once : 4251 )
#if defined _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#else
	#define PISTACHIO_API
#endif // PT_PLATFROM_WINDOWS

#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)


#ifdef __GNUC__
#define CDECL //__attribute__((__cdecl__))
#else
#define CDECL __cdecl
#endif

namespace Pistachio {
	template <typename T>
	using Scope =  std::unique_ptr<T>;
	template <typename T>
	using Ref = std::shared_ptr<T>;
	
	
}
#ifdef _DEBUG
#define GET_ASSERT_VER(_1, _2, NAME, ...) NAME
#define PT_CORE_ASSERT(...) GET_ASSERT_VER(__VA_ARGS__, PT_CORE_ASSERT2, PT_CORE_ASSERT1)(__VA_ARGS__)
#define PT_CORE_ASSERT1(cond) if(cond){}else{PT_CORE_ERROR("Assertion Failed");PT_DEBUG_BREAK}
#define PT_CORE_ASSERT2(cond, ...) if(cond){}else{PT_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__);PT_DEBUG_BREAK}
#else
#define PT_CORE_ASSERT(...) __VA_ARGS__
#endif // _DEBUG




