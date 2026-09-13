#include "modelpathresolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

QStringList ModelPathResolver::candidates(const QString& modelName)
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    QStringList paths;
    paths << applicationDir.filePath(modelName);
    paths << applicationDir.filePath("../" + modelName);
#ifdef INSTALLED
#ifdef Q_OS_WIN
    paths << applicationDir.filePath("models/" + modelName);
#else
    paths << "/usr/share/bg-remover/" + modelName;
#endif
#endif
    paths << applicationDir.filePath("../share/bg-remover/" + modelName);
    paths << QDir::current().filePath(modelName);
    paths.removeDuplicates();
    return paths;
}

QString ModelPathResolver::resolve(const QString& modelName)
{
    const QStringList paths = candidates(modelName);
    for (const QString& path : paths) {
        if (QFile::exists(path)) return path;
    }
    return paths.constFirst();
}
