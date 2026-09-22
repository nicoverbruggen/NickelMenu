#include "../src/compat.h"
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    assert(dlopen("libnickel.so.1.0.0", RTLD_LAZY | RTLD_GLOBAL));
    for (const auto &entry : nm_abi_functions) assert(nm_resolve(entry.name));
    assert(!nm_resolve("_missing_nickel_symbol"));
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
    // Mutate only this test process. A changed function must be rejected
    // before any private constructor or action is called.
    const char *name = "_ZN8SettingsC2ERK6Deviceb";
    auto *code = reinterpret_cast<unsigned char *>(reinterpret_cast<uintptr_t>(dlsym(RTLD_DEFAULT, name)) & ~uintptr_t(1));
    const auto pageSize = sysconf(_SC_PAGESIZE);
    void *page = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(code) & ~(uintptr_t(pageSize) - 1));
    assert(!mprotect(page, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC));
    *code ^= 1;
    assert(!nm_resolve(name));
    *code ^= 1;
    assert(nm_resolve(name));
    assert(!mprotect(page, pageSize, PROT_READ | PROT_EXEC));

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
    puts("PASS: private code, native sizes, missing symbols and changed code rejection");
}
