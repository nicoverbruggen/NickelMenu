#pragma once

#include "native.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

// A Qt6 relink can change code addresses without changing the calling
// interface. Check accessible code and the vtable layout we replace, not hashes.
inline void *nm_resolve(const char *name, const char **error = nullptr) {
    if (error) *error = nullptr;
    const auto reject = [error](const char *reason) -> void * {
        if (error) *error = reason;
        return nullptr;
    };
    void *address = dlsym(RTLD_DEFAULT, name);
    if (!address) return reject("symbol missing");
    if (std::strncmp(name, "_ZTV", 4)) {
        const auto *code = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(address) & ~uintptr_t(1));
        if (!nh_native_range(code, 2, PF_R | PF_X)) return reject("symbol is not readable executable code");
    }
    if (!std::strncmp(name, "_ZTV", 4)) {
        const QByteArray type = name + 4;
        Dl_info info{};
        void *extra = nullptr;
        if (!dladdr1(address, &info, &extra, RTLD_DL_SYMENT) || !extra) return reject("Settings vtable layout incompatible");
        const auto *symbol = static_cast<const ElfW(Sym) *>(extra);
        // Both inspected layouts have the Itanium header and five virtual
        // functions. Compare every slot before using a derived Settings type.
        if (symbol->st_size != 7 * sizeof(void *) || !nh_native_range(address, symbol->st_size, PF_R)) return reject("Settings vtable layout incompatible");
        const auto *entries = static_cast<void *const *>(address);
        const QByteArray names[] = {
            "_ZTI" + type, "_ZN" + type + "D1Ev", "_ZN" + type + "D0Ev",
            "_ZN8Settings12clearSectionEv",
            type == "15ReadingSettings" ? "_ZN15ReadingSettings11saveSettingERK7QStringRK8QVariantb"
                                        : "_ZN8Settings11saveSettingERK7QStringRK8QVariantb",
            "_ZNK" + type + "11sectionNameEv",
        };
        if (entries[0]) return reject("Settings vtable layout incompatible");
        for (size_t i = 0; i < 6; ++i) {
            void *expected = i ? nm_resolve(names[i].constData()) : dlsym(RTLD_DEFAULT, names[i].constData());
            if (!expected || entries[i + 1] != expected) return reject("Settings vtable layout incompatible");
        }
    }
    return address;
}

#else
inline void *nm_resolve(const char *name, const char **error = nullptr) {
    void *address = dlsym(RTLD_DEFAULT, name);
    if (error) *error = address ? nullptr : "symbol missing";
    return address;
}
#endif
