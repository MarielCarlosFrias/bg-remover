#ifndef SAVEPATH_H
#define SAVEPATH_H

#include <QFileInfo>
#include <QString>
#include <QStringList>

// Garante que o caminho termina com uma extensao coerente com o
// formato escolhido. Extensoes equivalentes (ex.: .jpeg para JPEG)
// sao aceites; qualquer outra e substituida pela extensao do formato.
// Sem extensao, a extensao do formato e acrescentada.
inline QString savePathWithExtension(
    const QString& path, const QString& extension,
    const QStringList& acceptedExtensions = QStringList())
{
    if (path.isEmpty() || extension.isEmpty()) return path;

    QStringList accepted;
    accepted << extension.toLower();
    for (const QString& e : acceptedExtensions)
        accepted << e.toLower();
    accepted.removeDuplicates();

    // "foto." e tratado como um nome sem extensao.
    if (path.endsWith('.'))
        return path + extension;

    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix.isEmpty())
        return path + "." + extension;
    if (accepted.contains(suffix))
        return path;

    const QString base = info.completeBaseName();
    if (base.isEmpty())
        return path + "." + extension;
    return info.dir().filePath(base + "." + extension);
}

#endif
