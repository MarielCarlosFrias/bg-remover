#include "modelpathresolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

QString ModelPathResolver::userModelsDir()
{
    // Caminho fixo, independente do nome da aplicacao. Antes usavamos
    // AppLocalDataLocation, o que fazia a GUI escrever em
    // .../bg-remover/bg-remover/models e o CLI em .../bg_remover_cli/models.
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation) + "/bg-remover/models";
}

QStringList ModelPathResolver::legacyUserModelsDirs()
{
    const QString generic = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    QStringList dirs;
    // Caminho antigo da GUI (org/app "bg-remover" duplicava o nome).
    dirs << generic + "/bg-remover/bg-remover/models";
    // Caminho antigo do CLI (app "bg_remover_cli").
    dirs << generic + "/bg_remover_cli/models";
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
    paths << "/usr/share/bg-remover/" + modelName;
#endif
#endif
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
