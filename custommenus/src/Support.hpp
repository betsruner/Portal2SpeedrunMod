#pragma once

#include <cstddef>
#include <cstdint>

// Platform glue, signature scanning, vtable hooking and logging for the menu system. Kept here
// rather than shared with the host plugin, so this folder builds on its own.

#ifdef _WIN32
#define MENUS_THISCALL __thiscall
#define MENUS_CDECL __cdecl
#define MENUS_MODULE(name) name ".dll"

#define MENUS_DECL_DETOUR(type, name, ...)                                \
    using _##name = type(MENUS_THISCALL*)(void* thisptr, ##__VA_ARGS__); \
    static _##name name;                                                  \
    static type __fastcall name##_Hook(void* thisptr, int edx, ##__VA_ARGS__);
#define MENUS_DETOUR(type, name, ...) \
    type __fastcall name##_Hook(void* thisptr, int edx, ##__VA_ARGS__)
#else
#define MENUS_THISCALL __attribute__((__cdecl__))
#define MENUS_CDECL __attribute__((__cdecl__))
#define MENUS_MODULE(name) name ".so"

#define MENUS_DECL_DETOUR(type, name, ...)                                \
    using _##name = type(MENUS_THISCALL*)(void* thisptr, ##__VA_ARGS__); \
    static _##name name;                                                  \
    static type MENUS_THISCALL name##_Hook(void* thisptr, ##__VA_ARGS__);
#define MENUS_DETOUR(type, name, ...) \
    type MENUS_THISCALL name##_Hook(void* thisptr, ##__VA_ARGS__)
#endif

#define MENUS_REDECL(name) decltype(name) name

// Defined when the menu system is built as its own module rather than into the plugin.
#ifdef MENUS_BUILD_MODULE
#ifdef _WIN32
#define MENUS_EXPORT extern "C" __declspec(dllexport)
#else
#define MENUS_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#else
#define MENUS_EXPORT extern "C"
#endif

namespace Menus::Support {

// Finds an IDA style pattern ("8B 0D ? ? ? ?") in a loaded module's code. Returns 0 if missing.
uintptr_t Scan(const char* module, const char* pattern, int offset = 0);
// Calls the module's CreateInterface. Returns nullptr if the module or interface is missing.
void* GetInterface(const char* module, const char* name);
void UnProtect(void* address, size_t length);

// The absolute target of a rel32 operand (a call or jmp's) at source.
template <typename T = uintptr_t>
inline T ReadRelative(uintptr_t source)
{
    return reinterpret_cast<T>(source + *reinterpret_cast<int*>(source) + sizeof(int));
}

// The game console, through tier0.
void Print(const char* format, ...);
void Warning(const char* format, ...);

// Gives one object its own copy of its vtable, so individual virtuals can be swapped out
// without touching any other object of the class.
class VtableHook {
public:
    VtableHook(void* object, int size);
    ~VtableHook();
    VtableHook(const VtableHook&) = delete;
    VtableHook& operator=(const VtableHook&) = delete;

    template <typename T, typename U>
    void Hook(int index, T detour, U& original)
    {
        original = reinterpret_cast<U>(this->original[index]);
        this->copy[index + Prefix] = reinterpret_cast<uintptr_t>(detour);
    }

    // The object was freed, so don't write its vtable back.
    void Forget() { this->object = nullptr; }

private:
    // The entries before the vtable that go with it. MSVC has the RTTI locator; GCC has the
    // typeinfo, the offset to top, and before those the virtual base and virtual call offsets
    // vgui::Panel's virtual base needs.
#ifdef _WIN32
    static constexpr int Prefix = 1;
#else
    static constexpr int Prefix = 8;
#endif

    uintptr_t** object;
    uintptr_t* original;
    uintptr_t* copy; // starts with the Prefix entries
};

}
