#include "../src/compat.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
struct NmAbiFunction { const char *name; size_t size; const char *sha256; };
#include "../src/compat_data.h"
#endif
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    assert(dlopen("libnickel.so.1.0.0", RTLD_LAZY | RTLD_GLOBAL));
    for (const auto &entry : nm_abi_functions) assert(nm_resolve(entry.name));
    const char *error = nullptr;
    assert(!nm_resolve("_missing_nickel_symbol", &error));
    assert(!std::strcmp(error, "symbol missing"));
    assert(!nm_resolve("_ZTI15FeatureSettings", &error));
    assert(!std::strcmp(error, "symbol is not readable executable code"));
    assert(!nh_native_range(nullptr, 8, PF_R));
    assert(!nh_native_range(reinterpret_cast<void *>(1), SIZE_MAX, PF_R));
    for (const char *name : {"MainNavButton", "MenuTextItem", "NickelTouchMenu", "MoreController",
            "BrowserWorkflowManager", "LightMenuSeparator", "BoldMenuSeparator", "SettingItemWithCheckBox"}) {
        assert(nh_native_type(name).isValid());
        void *storage = nh_native_storage(name);
        assert(storage);
        ::operator delete(storage);
    }
    assert(!nh_native_storage("MissingNativeWidget"));
    const auto pageSize = sysconf(_SC_PAGESIZE);
    // Never execute changed code. Qt6 must resolve functions even when a
    // relink changes constructor literals or navigation method bytes.
    for (const char *name : {"_ZN8SettingsC2ERK6Deviceb", "_ZN19ReadingLifeNavMixinC1Ev",
            "_ZN19ReadingLifeNavMixin5statsEv", "_ZN19ReadingLifeNavMixin14chooseActivityEv"}) {
        auto *code = reinterpret_cast<unsigned char *>(reinterpret_cast<uintptr_t>(dlsym(RTLD_DEFAULT, name)) & ~uintptr_t(1));
        assert(code);
        const size_t offset = !std::strcmp(name, "_ZN19ReadingLifeNavMixinC1Ev") ? 16 : 0;
        void *page = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(code + offset) & ~(uintptr_t(pageSize) - 1));
        assert(!mprotect(page, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC));
        code[offset] ^= 1;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        assert(nm_resolve(name));
#else
        assert(!nm_resolve(name));
#endif
        code[offset] ^= 1;
        assert(nm_resolve(name, &error));
        assert(!error);
        assert(!mprotect(page, pageSize, PROT_READ | PROT_EXEC));
    }

    auto *iface = const_cast<QtPrivate::QMetaTypeInterface *>(nh_native_type("MainNavButton").iface());
    const auto size = iface->size;
    iface->size = 0;
    assert(!nh_native_storage("MainNavButton"));
    iface->size = size;
    assert(nh_native_type("MainNavButton").isValid());
    auto **vtable = static_cast<void **>(nm_resolve("_ZTV15FeatureSettings"));
    assert(vtable);
    void *vtablePage = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(vtable) & ~(uintptr_t(pageSize) - 1));
    // Firmware places vtables in RELRO memory. Only this rejection fixture
    // makes the page writable to simulate a changed virtual function slot.
    assert(!mprotect(vtablePage, pageSize, PROT_READ | PROT_WRITE));
    void *originalSlot = vtable[4];
    vtable[4] = vtable[2];
    assert(!nm_resolve("_ZTV15FeatureSettings"));
    vtable[4] = originalSlot;
    assert(nm_resolve("_ZTV15FeatureSettings"));
    assert(!mprotect(vtablePage, pageSize, PROT_READ));
    puts("PASS: symbol resolution, native sizes and Settings vtable guards");
}
