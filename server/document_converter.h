#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>

#ifdef Q_OS_WIN
class QAxObject;
#endif

namespace CrossNetShare {

class DocumentConverter {
public:
    struct PreviewResult {
        bool success;
        QString mimeType;
        QByteArray data;
        QString error;
    };

    static void initialize();
    static void cleanup();
    static void cleanupCache();

    static PreviewResult previewFile(const QString& filePath);
    static bool isPreviewSupported(const QString& filePath);
    static QString findPython();

    // 直接将 Word 文档转换为高质量 JPG 图片（绕过 PDF）
    static QString convertWordToJpg(const QString& filePath, const QString& outputDir);

private:
    static PreviewResult previewText(const QString& filePath);
    static PreviewResult previewImage(const QString& filePath);
    static PreviewResult previewPdf(const QString& filePath);
    static PreviewResult previewWord(const QString& filePath);
    static PreviewResult convertWordWithAspose(const QString& filePath);
    static QString findAsposeLicense();
    static QByteArray htmlEscape(const QString& text);

    static QString getCachePath(const QString& filePath);
    static bool isCacheValid(const QString& cachePath, const QString& originalPath);
};

}
