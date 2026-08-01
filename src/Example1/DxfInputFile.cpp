#include "DxfInputFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>

namespace {

const char kStagingDirectoryPrefix[] = ".dxf-import-";

QStringList stagingRootCandidates()
{
    QStringList roots;
    roots << QDir::tempPath();
    if (QCoreApplication::instance()) {
        roots << QCoreApplication::applicationDirPath();
    }
    roots << QDir::currentPath();
    roots.removeDuplicates();
    return roots;
}

bool isLocalPathRoundTripSafe(const QString& path, QByteArray& encodedPath)
{
    encodedPath = QDir::toNativeSeparators(path).toLocal8Bit();
    return QString::fromLocal8Bit(encodedPath) == QDir::toNativeSeparators(path);
}

} // namespace

DxfInputFile::DxfInputFile(const QString& sourcePath)
    : m_sourcePath(QFileInfo(sourcePath).absoluteFilePath())
{
}

DxfInputFile::~DxfInputFile() = default;

bool DxfInputFile::setRepresentablePath(const QString& path)
{
    QByteArray encodedPath;
    if (!isLocalPathRoundTripSafe(path, encodedPath)) {
        return false;
    }
    m_encodedPath = encodedPath;
    return true;
}

bool DxfInputFile::prepare()
{
    m_encodedPath.clear();
    m_errorMessage.clear();
    m_stagingDir.reset();

    const QFileInfo sourceInfo(m_sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        m_errorMessage = QStringLiteral("DXF file does not exist or is not a file");
        return false;
    }

    if (setRepresentablePath(m_sourcePath)) {
        return true;
    }

    for (const QString& root : stagingRootCandidates()) {
        QByteArray ignored;
        if (!isLocalPathRoundTripSafe(root, ignored)) {
            continue;
        }

        const QString pattern = QDir(root).filePath(
            QString::fromLatin1(kStagingDirectoryPrefix) + QStringLiteral("XXXXXX"));
        std::unique_ptr<QTemporaryDir> candidate(new QTemporaryDir(pattern));
        if (!candidate->isValid()) {
            continue;
        }

        const QString stagedPath = QDir(candidate->path()).filePath(
            QStringLiteral("input.dxf"));
        if (!setRepresentablePath(stagedPath)) {
            continue;
        }
        if (!QFile::copy(m_sourcePath, stagedPath)) {
            m_encodedPath.clear();
            continue;
        }

        const QFileInfo stagedInfo(stagedPath);
        if (!stagedInfo.isFile() || stagedInfo.size() != sourceInfo.size()) {
            QFile::remove(stagedPath);
            m_encodedPath.clear();
            continue;
        }

        m_stagingDir = std::move(candidate);
        return true;
    }

    m_errorMessage = QStringLiteral(
        "Failed to create a temporary path that libdxfrw can read");
    return false;
}
