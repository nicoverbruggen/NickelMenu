#pragma once

#include "native.h"
#include <QCryptographicHash>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
struct NmAbiFunction { const char *name; size_t size; const char *sha256; };
#include "compat_data.h"

// Settings and navigation mixins lack allocating factories. Their callers
// retain the inspected storage layouts only while the relevant code matches.
inline void *nm_resolve(const char *name) {
    void *address = dlsym(RTLD_DEFAULT, name);
    if (!address) return nullptr;
    if (std::strncmp(name, "_ZTV", 4)) {
        const auto *code = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(address) & ~uintptr_t(1));
        if (!nh_native_range(code, 2, PF_R | PF_X)) return nullptr;
    }
    bool guarded = false;
    for (const auto &entry : nm_abi_functions) {
        if (std::strcmp(name, entry.name)) continue;
        guarded = true;
        Dl_info info{};
        void *extra = nullptr;
        if (!dladdr1(address, &info, &extra, RTLD_DL_SYMENT) || !extra) continue;
        const auto *symbol = static_cast<const ElfW(Sym) *>(extra);
        const auto *code = reinterpret_cast<const char *>(reinterpret_cast<uintptr_t>(address) & ~uintptr_t(1));
        if (symbol->st_size != entry.size || !nh_native_range(code, entry.size, PF_R | PF_X)) continue;
        if (QCryptographicHash::hash(QByteArray(code, entry.size), QCryptographicHash::Sha256).toHex() == entry.sha256)
            return address;
    }
    if (guarded) return nullptr;
    // A newly exported method in one of these private classes has not been
    // checked merely because it was absent from both reference images.
    for (const char *prefix : {"8Settings", "15FeatureSettings", "15ReadingSettings", "13PowerSettings",
            "11DevSettings", "19ApplicationSettings", "16DiscoverNavMixin", "15LibraryNavMixin",
            "19ReadingLifeNavMixin", "13StoreNavMixin"}) {
        const QByteArray normal = QByteArray("_ZN") + prefix;
        const QByteArray constant = QByteArray("_ZNK") + prefix;
        if (QByteArray(name).startsWith(normal) || QByteArray(name).startsWith(constant)) return nullptr;
    }
    if (!std::strncmp(name, "_ZTV", 4)) {
        const QByteArray type = name + 4;
        Dl_info info{};
        void *extra = nullptr;
        if (!dladdr1(address, &info, &extra, RTLD_DL_SYMENT) || !extra) return nullptr;
        const auto *symbol = static_cast<const ElfW(Sym) *>(extra);
        // Both inspected layouts have the Itanium header and five virtual
        // functions. Compare every slot before using a derived Settings type.
        if (symbol->st_size != 7 * sizeof(void *) || !nh_native_range(address, symbol->st_size, PF_R)) return nullptr;
        const auto *entries = static_cast<void *const *>(address);
        const QByteArray names[] = {
            "_ZTI" + type, "_ZN" + type + "D1Ev", "_ZN" + type + "D0Ev",
            "_ZN8Settings12clearSectionEv",
            type == "15ReadingSettings" ? "_ZN15ReadingSettings11saveSettingERK7QStringRK8QVariantb"
                                        : "_ZN8Settings11saveSettingERK7QStringRK8QVariantb",
            "_ZNK" + type + "11sectionNameEv",
        };
        if (entries[0]) return nullptr;
        for (size_t i = 0; i < 6; ++i) {
            void *expected = i ? nm_resolve(names[i].constData()) : dlsym(RTLD_DEFAULT, names[i].constData());
            if (!expected || entries[i + 1] != expected) return nullptr;
        }
    }
    return address;
}

#else
inline void *nm_resolve(const char *name) {
    return dlsym(RTLD_DEFAULT, name);
}
#endif
