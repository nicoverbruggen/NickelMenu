#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

extern "C" {
#include <NickelHook.h>
#include <resources.h>
}
#include "embedded_resources.h"

const struct nh_resources nh_resources = {
    .config_dir = NM_CONFIG_DIR,
    .uninstall_file = NM_CONFIG_DIR "/uninstall",
    .files = nh_embedded_resources,
};

static bool replaceFile(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

// NickelHook seeds the directory at priority 200. Its receipt used to contain
// "1". Recording the release here also updates existing installations once.
__attribute__((constructor(201))) static void updateDoc() {
    const QFileInfo config(QStringLiteral(NM_CONFIG_DIR));
    const QString receiptPath = config.dir().filePath("." + config.fileName() + ".initialized");
    QFile receipt(receiptPath);
    if (!config.isDir() || QFileInfo::exists(QStringLiteral(NM_CONFIG_DIR "/uninstall")) ||
            QFileInfo(receipt).isSymLink() || !QFileInfo(receipt).isFile() || !receipt.open(QIODevice::ReadOnly))
        return;

    const auto &doc = nh_embedded_resources[0];
#ifdef NH_VERSION
    const QByteArray version = "NickelMenu " NH_VERSION "\n";
#else
    const QByteArray version = "NickelMenu unversioned\n";
#endif
    if (receipt.read(version.size() + 1) == version) return;
    receipt.close();

    const QString path = QStringLiteral(NM_CONFIG_DIR "/doc");
    const QFileInfo target(path);
    if (target.isSymLink() || (target.exists() && !target.isFile())) {
        nh_log("cannot update doc: target is not a regular file");
        return;
    }
    const auto contents = QByteArray::fromRawData(reinterpret_cast<const char *>(doc.data), doc.size);
    if (!replaceFile(path, contents) || !replaceFile(receiptPath, version))
        nh_log("could not update documentation; retrying on next start");
}
