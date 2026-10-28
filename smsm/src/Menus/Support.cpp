#include "Support.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
// Last
#include <psapi.h>
#else
#include <dlfcn.h>
#include <link.h>
#include <sys/mman.h>
#endif

namespace Menus::Support {

namespace {
struct CodeRange {
    uintptr_t start = 0;
    uintptr_t end = 0;
};

#ifdef _WIN32
void* OpenModule(const char* module)
{
    return GetModuleHandleA(module);
}

void* Symbol(void* handle, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

void CloseModule(void*) {}

CodeRange FindCode(const char* module)
{
    MODULEINFO info {};
    HMODULE handle = GetModuleHandleA(module);
    if (!handle || !GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(info)))
        return {};
    const auto base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
    return { base, base + info.SizeOfImage };
}
#else
void* OpenModule(const char* module)
{
    return dlopen(module, RTLD_NOLOAD | RTLD_NOW);
}

void* Symbol(void* handle, const char* name)
{
    return dlsym(handle, name);
}

void CloseModule(void* handle)
{
    dlclose(handle);
}

CodeRange FindCode(const char* module)
{
    struct Search {
        const char* module;
        CodeRange range;
    } search { module, {} };
    dl_iterate_phdr([](dl_phdr_info* info, size_t, void* data) {
        auto search = static_cast<Search*>(data);
        const char* slash = std::strrchr(info->dlpi_name, '/');
        if (std::strcmp(slash ? slash + 1 : info->dlpi_name, search->module))
            return 0;
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            if (info->dlpi_phdr[i].p_flags & PF_X) {
                search->range.start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
                search->range.end = search->range.start + info->dlpi_phdr[i].p_memsz;
                return 1;
            }
        }
        return 0;
    },
        &search);
    return search.range;
}
#endif

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

// -1 for a wildcard byte.
int ParsePattern(const char* pattern, int* bytes, int capacity)
{
    int count = 0;
    for (const char* c = pattern; *c && count < capacity;) {
        if (*c == ' ') {
            ++c;
        } else if (*c == '?') {
            bytes[count++] = -1;
            while (*c == '?')
                ++c;
        } else if (HexDigit(c[0]) >= 0 && HexDigit(c[1]) >= 0) {
            bytes[count++] = HexDigit(c[0]) << 4 | HexDigit(c[1]);
            c += 2;
        } else {
            return 0;
        }
    }
    return count;
}

using LogFunction = void(MENUS_CDECL*)(const char* format, ...);

LogFunction Tier0(const char* name)
{
#ifdef _WIN32
    void* tier0 = OpenModule("tier0.dll");
#else
    void* tier0 = OpenModule("libtier0.so");
#endif
    auto function = tier0 ? reinterpret_cast<LogFunction>(Symbol(tier0, name)) : nullptr;
    if (tier0)
        CloseModule(tier0);
    return function;
}

void Log(bool warning, const char* format, va_list args)
{
    static const LogFunction msgFunction = Tier0("Msg");
    static const LogFunction warningFunction = Tier0("Warning");
    const LogFunction function = warning ? warningFunction : msgFunction;
    if (!function)
        return;
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    function("%s", buffer);
}
}

uintptr_t Scan(const char* module, const char* pattern, int offset)
{
    int bytes[256];
    const int length = ParsePattern(pattern, bytes, 256);
    const CodeRange range = FindCode(module);
    if (!length || !range.start)
        return 0;

    for (uintptr_t position = range.start; position + length <= range.end; ++position) {
        int matched = 0;
        while (matched < length && (bytes[matched] < 0 || bytes[matched] == *reinterpret_cast<const unsigned char*>(position + matched)))
            ++matched;
        if (matched == length)
            return position + offset;
    }
    return 0;
}

void* GetInterface(const char* module, const char* name)
{
    using CreateInterfaceFn = void*(MENUS_CDECL*)(const char* name, int* returnCode);
    void* handle = OpenModule(module);
    if (!handle)
        return nullptr;
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(Symbol(handle, "CreateInterface"));
    CloseModule(handle);

    int returnCode = 0;
    void* result = createInterface ? createInterface(name, &returnCode) : nullptr;
    return returnCode == 0 ? result : nullptr;
}

void UnProtect(void* address, size_t length)
{
    const uintptr_t start = reinterpret_cast<uintptr_t>(address) & ~uintptr_t(0xFFF);
    const uintptr_t end = (reinterpret_cast<uintptr_t>(address) + length + 0xFFF) & ~uintptr_t(0xFFF);
#ifdef _WIN32
    DWORD previous;
    VirtualProtect(reinterpret_cast<void*>(start), end - start, PAGE_EXECUTE_READWRITE, &previous);
#else
    mprotect(reinterpret_cast<void*>(start), end - start, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
}

void Print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Log(false, format, args);
    va_end(args);
}

void Warning(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Log(true, format, args);
    va_end(args);
}

VtableHook::VtableHook(void* object, int size)
    : object(static_cast<uintptr_t**>(object))
    , original(*static_cast<uintptr_t**>(object))
    , copy(new uintptr_t[size + 1])
{
    std::memcpy(this->copy, this->original - 1, (size + 1) * sizeof(uintptr_t));
    *this->object = this->copy + 1;
}

VtableHook::~VtableHook()
{
    if (this->object && *this->object == this->copy + 1)
        *this->object = this->original;
    delete[] this->copy;
}

}
