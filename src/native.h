#pragma once

#include <QMetaType>
#include <QByteArray>
#include <QObject>
#include <cstddef>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <new>

// Inspect ELF mappings before reading private firmware metadata. A symbol's
// presence alone does not establish that the required bytes are accessible.
inline bool nh_native_range(const void *address, size_t size, unsigned flags) {
    struct Range { uintptr_t address; size_t size; unsigned flags; bool found; };
    Range range{reinterpret_cast<uintptr_t>(address), size, flags, false};
    if (!address || !size) return false;
    dl_iterate_phdr([](dl_phdr_info *info, size_t, void *opaque) {
        auto &r = *static_cast<Range *>(opaque);
        for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
            const auto &p = info->dlpi_phdr[i];
            if (p.p_type != PT_LOAD || (p.p_flags & r.flags) != r.flags) continue;
            const uintptr_t start = info->dlpi_addr + p.p_vaddr;
            if (r.address >= start && r.address - start < p.p_memsz
                    && r.size <= p.p_memsz - (r.address - start)) {
                r.found = true;
                return 1;
            }
        }
        return 0;
    }, &range);
    return range.found;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline QMetaType nh_native_type(const char *name) {
    const QByteArray symbol = "_ZN9QtPrivate25QMetaTypeInterfaceWrapperI"
        + QByteArray::number(std::strlen(name)) + name + "E8metaTypeE";
    const auto *iface = static_cast<const QtPrivate::QMetaTypeInterface *>(
        dlsym(RTLD_DEFAULT, symbol.constData()));
    if (!nh_native_range(iface, sizeof(*iface), PF_R)) return {};
    // These plugins are built against this Qt metadata ABI. Reject a changed
    // revision instead of interpreting new fields with the old layout.
    if (iface->revision != QMetaType::fromType<QObject *>().iface()->revision
            || !iface->size || iface->size > 1024 * 1024
            || !iface->alignment || iface->alignment > alignof(std::max_align_t)
            || (iface->alignment & (iface->alignment - 1))) return {};
    return QMetaType(iface);
}

inline void *nh_native_storage(const char *name) {
    const auto type = nh_native_type(name);
    if (!type.isValid()) return nullptr;
    void *storage = ::operator new(type.sizeOf(), std::nothrow);
    if (storage) std::memset(storage, 0, type.sizeOf());
    return storage;
}

#endif

inline void *nm_native_storage(const char *name, size_t qt5_size) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    (void)qt5_size;
    return nh_native_storage(name);
#else
    (void)name;
    void *storage = ::operator new(qt5_size, std::nothrow);
    if (storage) std::memset(storage, 0, qt5_size);
    return storage;
#endif
}
