#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <vector>

namespace CrossNetShare {

class WatermarkService : public QObject {
    Q_OBJECT

public:
    // 水印配置
    struct WatermarkConfig {
        int fontSize;           // 字号
        QColor color;           // 颜色
        int opacity;            // 透明度 0-100
        int rotation;           // 旋转角度
        QString density;        // 密度: sparse/medium/dense
        bool enableUnified;     // 启用统一水印
        QString unifiedText;    // 统一水印文字
        bool enabled;           // 全局启用水印功能
        bool zipDownload;       // 打包下载（true=ZIP, false=逐个下载）

        WatermarkConfig()
            : fontSize(14)
            , color(128, 128, 128)  // 灰色
            , opacity(30)
            , rotation(45)
            , density("medium")
            , enableUnified(false)
            , enabled(false)
            , zipDownload(true)     // 默认打包下载
        {}
    };

    // 关键词规则
    struct KeywordRule {
        QString detectText;     // 检测文本
        QString watermarkText;  // 水印文本
        bool enabled;           // 是否启用

        KeywordRule(const QString& detect = "", const QString& watermark = "", bool en = true)
            : detectText(detect), watermarkText(watermark), enabled(en) {}
    };

    // 水印生成结果
    struct WatermarkResult {
        bool success;
        QStringList generatedFiles;  // 生成的图片文件名（不含路径）
        QString zipFilePath;         // ZIP 文件路径
        QString error;

        WatermarkResult() : success(false) {}
    };

    explicit WatermarkService(QObject* parent = nullptr);
    ~WatermarkService();

    // 加载/保存配置
    bool loadConfig(const QString& configPath = "");
    bool saveConfig(const QString& configPath = "");

    // 配置访问
    WatermarkConfig getConfig() const { return config_; }
    void setConfig(const WatermarkConfig& config);

    QList<KeywordRule> getKeywords() const { return keywords_; }
    void setKeywords(const QList<KeywordRule>& keywords);

    // 生成带水印的图片
    // suggestionTextOverride: 非空时直接使用该文本进行关键词匹配（批办单模式
    // 直接传入数据库中的 SUGGESTION 字段，避免依赖从 Word 文档提取文本）
    WatermarkResult generateWatermarkedImages(
        const QString& wordFilePath,
        const QString& outputDir,
        const QString& originalFileName,
        const QString& suggestionTextOverride = QString()
    );

signals:
    void progress(const QString& message, int current, int total);
    void logMessage(const QString& message);

private:
    // Word 文档处理
    QString convertWordToPdf(const QString& wordFilePath, const QString& outputDir);
    QString convertPdfToJpg(const QString& pdfFilePath, const QString& outputDir);
    QString convertWordToHtml(const QString& wordFilePath, const QString& outputDir);
    QString convertDocumentToJpg(const QString& wordFilePath, const QString& outputDir);

    // 表格解析
    QString extractOpinionText(const QString& htmlFilePath);

    // 直接从 Word 文档提取"建议："段落
    QString extractSuggestionFromWord(const QString& wordFilePath);

    // 关键词匹配
    QList<KeywordRule> matchKeywords(const QString& text);

    // 水印绘制
    QImage addWatermark(const QImage& source, const QString& text);

    // 打包 ZIP
    QString createZipFile(const QStringList& imagePaths, const QString& outputDir, const QString& baseName);

    // 配置
    WatermarkConfig config_;
    QList<KeywordRule> keywords_;
    QString configPath_;
};

}
