#include "modelpathresolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

QString ModelPathResolver::userModelsDir()
{
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation) + "/aletheia/models";
}

QStringList ModelPathResolver::legacyUserModelsDirs()
{
    const QString generic = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    QStringList dirs;
    // Caminhos antigos do bg-remover para compatibilidade perfeita
    dirs << generic + "/bg-remover/models";
    dirs << generic + "/bg-remover/bg-remover/models";
    dirs << generic + "/bg_remover_cli/models";
    dirs << generic + "/aletheia/aletheia/models";
    dirs << generic + "/aletheia_cli/models";
    // Variacoes de plataforma do AppLocalDataLocation.
    dirs << QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation) + "/models";
    dirs.removeDuplicates();
    dirs.removeAll(userModelsDir());
    return dirs;
}

QStringList ModelPathResolver::findAllUserModels(const QString& modelName)
{
    QStringList paths;
    const QString canonical = userModelsDir() + "/" + modelName;
    if (QFile::exists(canonical)) paths << canonical;
    for (const QString& dir : legacyUserModelsDirs()) {
        const QString path = dir + "/" + modelName;
        if (QFile::exists(path)) paths << path;
    }
    paths.removeDuplicates();
    return paths;
}

QString ModelPathResolver::findUserModel(const QString& modelName)
{
    const QStringList paths = findAllUserModels(modelName);
    return paths.isEmpty() ? QString() : paths.first();
}

QStringList ModelPathResolver::candidates(const QString& modelName)
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    QStringList paths;
    paths << applicationDir.filePath(modelName);
    paths << applicationDir.filePath("../" + modelName);
#ifdef Q_OS_MACOS
    // App bundle: Contents/MacOS/bg_remover_gui -> Contents/Resources.
    paths << applicationDir.filePath("../Resources/" + modelName);
#endif
#ifdef INSTALLED
#ifdef Q_OS_WIN
    paths << applicationDir.filePath("models/" + modelName);
#else
    paths << "/usr/share/aletheia/" + modelName;
    paths << "/usr/share/bg-remover/" + modelName;
#endif
#endif
    paths << applicationDir.filePath("../share/aletheia/" + modelName);
    paths << applicationDir.filePath("../share/bg-remover/" + modelName);
    paths << userModelsDir() + "/" + modelName;
    for (const QString& dir : legacyUserModelsDirs())
        paths << dir + "/" + modelName;
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
