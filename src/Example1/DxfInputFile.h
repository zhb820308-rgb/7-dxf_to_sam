#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

class QTemporaryDir;

// Adapts a Unicode Qt path to libdxfrw's narrow-path-only API. Paths that
// survive the local-code-page round trip are used directly. Other paths are
// copied to a unique representable staging directory for the duration of the
// object.
class DxfInputFile
{
public:
    explicit DxfInputFile(const QString& sourcePath);
    ~DxfInputFile();

    DxfInputFile(const DxfInputFile&) = delete;
    DxfInputFile& operator=(const DxfInputFile&) = delete;

    bool prepare();
    const QByteArray& encodedPath() const { return m_encodedPath; }
    const QString& errorMessage() const { return m_errorMessage; }
    bool usesStagingCopy() const { return static_cast<bool>(m_stagingDir); }

private:
    bool setRepresentablePath(const QString& path);

    QString m_sourcePath;
    QByteArray m_encodedPath;
    QString m_errorMessage;
    std::unique_ptr<QTemporaryDir> m_stagingDir;
};
