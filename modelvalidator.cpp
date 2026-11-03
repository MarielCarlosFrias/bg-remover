#include "modelvalidator.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

ModelFileValidation validateModelFile(
    const QString& path, qint64 expectedSize,
    const QString& expectedSha256Hex)
{
    ModelFileValidation result;

    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile())
        return result;

    result.actualSize = info.size();
    if (expectedSize > 0) {
        if (result.actualSize < expectedSize) {
            result.state = ModelFileState::Incomplete;
            return result;
        }
        if (result.actualSize != expectedSize) {
            result.state = ModelFileState::Corrupt;
            return result;
        }
    }

    if (!expectedSha256Hex.isEmpty()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            result.state = ModelFileState::Corrupt;
            return result;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) {
            result.state = ModelFileState::Corrupt;
            return result;
        }
        const QString actual =
            QString::fromLatin1(hash.result().toHex());
        if (actual.compare(expectedSha256Hex,
                           Qt::CaseInsensitive) != 0) {
            result.state = ModelFileState::Corrupt;
            return result;
        }
    }

    result.state = ModelFileState::Installed;
    return result;
}
