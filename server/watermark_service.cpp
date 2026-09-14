#include "watermark_service.h"
#include "document_converter.h"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QPainter>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QDebug>
#include <QImageReader>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 调试宏：同时输出到控制台和日志信号
#define LOG_MESSAGE(msg) do { \
    QString logMsg = (msg); \
    qDebug() << "[Watermark]" << logMsg; \
    emit logMessage(logMsg); \
} while(0)

namespace CrossNetShare {

// 辅助函数：配置 QProcess 在 Windows 上隐藏窗口
static void configureProcessNoWindow(QProcess* process) {
#ifdef Q_OS_WIN
    // Windows: 隐藏控制台窗口
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#else
    Q_UNUSED(process);
#endif
}

WatermarkService::WatermarkService(QObject* parent)
    : QObject(parent)
{
    // 设置默认配置路径
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    configPath_ = appDataPath + "/watermark_config.json";

    // 加载配置
    loadConfig();
}

WatermarkService::~WatermarkService() {
    saveConfig();
}

bool WatermarkService::loadConfig(const QString& configPath) {
    QString path = configPath.isEmpty() ? configPath_ : configPath;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage("Watermark config not found, using defaults");

        // 设置默认关键词
        keywords_ = {
            KeywordRule("党政办", "党政办", true),
            KeywordRule("医政科", "医政科", true),
            KeywordRule("公卫科", "公卫科", true),
            KeywordRule("中医科", "中医科", true),
            KeywordRule("法监科", "法监科", true),
            KeywordRule("家庭发展科", "家庭发展科", true),
            KeywordRule("健康促进科（爱卫办）", "健康促进科", true),
            KeywordRule("办公室（后勤）", "办公室（后勤）", true),
            KeywordRule("办公室（财务）", "办公室（财务）", true),
            KeywordRule("信息中心", "信息中心", true),
            KeywordRule("计生协", "计生协", true),
            KeywordRule("疾控中心（监督所、健教所）", "疾控中心", true),
            KeywordRule("妇计中心", "妇计中心", true),
            KeywordRule("中心医院", "中心医院", true),
            KeywordRule("大雁塔", "大雁塔", true),
            KeywordRule("小寨路", "小寨路", true),
            KeywordRule("长延堡", "长延堡", true),
            KeywordRule("电子城", "电子城", true),
            KeywordRule("等驾坡", "等驾坡", true),
            KeywordRule("曲江", "曲江", true),
            KeywordRule("杜城", "杜城", true),
            KeywordRule("各科室", "各科室", true),
            KeywordRule("各单位", "各单位", true),
            KeywordRule("各科室、各单位", "全体科室单位", true)
        };

        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit logMessage("Invalid watermark config format");
        return false;
    }

    QJsonObject root = doc.object();

    // 加载水印样式
    if (root.contains("watermarkStyle")) {
        QJsonObject style = root["watermarkStyle"].toObject();
        config_.fontSize = style["fontSize"].toInt(14);
        config_.color = QColor(style["color"].toString("#808080"));
        config_.opacity = style["opacity"].toInt(30);
        config_.rotation = style["rotation"].toInt(45);
        config_.density = style["density"].toString("medium");
        config_.enableUnified = style["enableUnified"].toBool(false);
        config_.unifiedText = style["unifiedText"].toString("");
        config_.enabled = style["enabled"].toBool(false);
        config_.zipDownload = style["zipDownload"].toBool(true);  // 默认打包下载
    }

    // 加载关键词
    keywords_.clear();
    if (root.contains("keywords")) {
        QJsonArray keywordsArray = root["keywords"].toArray();
        for (const QJsonValue& val : keywordsArray) {
            QJsonObject obj = val.toObject();
            KeywordRule rule;
            rule.detectText = obj["detectText"].toString();
            rule.watermarkText = obj["watermarkText"].toString();
            rule.enabled = obj["enabled"].toBool(true);
            keywords_.append(rule);
        }
    }

    emit logMessage("Watermark config loaded successfully");
    return true;
}

bool WatermarkService::saveConfig(const QString& configPath) {
    QString path = configPath.isEmpty() ? configPath_ : configPath;

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QJsonObject root;

    // 保存水印样式
    QJsonObject style;
    style["fontSize"] = config_.fontSize;
    style["color"] = config_.color.name();
    style["opacity"] = config_.opacity;
    style["rotation"] = config_.rotation;
    style["density"] = config_.density;
    style["enableUnified"] = config_.enableUnified;
    style["unifiedText"] = config_.unifiedText;
    style["enabled"] = config_.enabled;
    style["zipDownload"] = config_.zipDownload;
    root["watermarkStyle"] = style;

    // 保存关键词
    QJsonArray keywordsArray;
    for (const KeywordRule& rule : keywords_) {
        QJsonObject obj;
        obj["detectText"] = rule.detectText;
        obj["watermarkText"] = rule.watermarkText;
        obj["enabled"] = rule.enabled;
        keywordsArray.append(obj);
    }
    root["keywords"] = keywordsArray;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        emit logMessage("Failed to save watermark config: " + file.errorString());
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit logMessage("Watermark config saved successfully");
    return true;
}

void WatermarkService::setConfig(const WatermarkConfig& config) {
    config_ = config;
}

void WatermarkService::setKeywords(const QList<KeywordRule>& keywords) {
    keywords_ = keywords;
}

WatermarkService::WatermarkResult WatermarkService::generateWatermarkedImages(
    const QString& wordFilePath,
    const QString& outputDir,
    const QString& originalFileName)
{
    WatermarkResult result;

    emit progress("开始处理文档...", 1, 5);
    LOG_MESSAGE("Processing: " + wordFilePath);

    // 1. 转换 Word 为 PDF（使用 DocumentConverter，处理 WordML 格式）
    emit progress("转换文档为 PDF...", 2, 5);
    QString pdfPath = convertWordToPdf(wordFilePath, outputDir);
    if (pdfPath.isEmpty()) {
        result.error = "Failed to convert Word to PDF";
        return result;
    }

    // 2. 直接从 Word 文档提取"建议："段落
    emit progress("提取建议内容...", 3, 5);
    QString suggestionText = extractSuggestionFromWord(wordFilePath);
    if (suggestionText.isEmpty()) {
        LOG_MESSAGE("Warning: No suggestion text found (no '建议：' paragraph)");
    }

    // 3. 匹配关键词
    QList<KeywordRule> matchedRules;
    if (config_.enableUnified) {
        // 统一水印模式：使用配置的统一文字
        if (!config_.unifiedText.isEmpty()) {
            KeywordRule unifiedRule;
            unifiedRule.detectText = "统一水印";
            unifiedRule.watermarkText = config_.unifiedText;
            unifiedRule.enabled = true;
            matchedRules.append(unifiedRule);
        }
    } else {
        // 关键词模式：匹配检测到的关键词
        if (!suggestionText.isEmpty()) {
            matchedRules = matchKeywords(suggestionText);
        }
    }

    if (matchedRules.isEmpty()) {
        // 没有匹配到关键词，生成无水印原图
        LOG_MESSAGE("No keywords matched, generating original image");
        KeywordRule emptyRule;
        emptyRule.detectText = "";
        emptyRule.watermarkText = "";
        emptyRule.enabled = true;
        matchedRules.append(emptyRule);
    }

    // 4. 转换文档为图片（优先使用Word COM直接渲染，保证字体正确）
    emit progress("转换文档为图片...", 4, 5);
    QString baseImagePath = convertDocumentToJpg(wordFilePath, outputDir);
    if (baseImagePath.isEmpty()) {
        result.error = "Failed to convert document to JPG";
        return result;
    }

    LOG_MESSAGE("Loading image from: " + baseImagePath);
    LOG_MESSAGE("File exists: " + QString(QFile::exists(baseImagePath) ? "Yes" : "No"));
    if (QFile::exists(baseImagePath)) {
        QFileInfo fileInfo(baseImagePath);
        LOG_MESSAGE("File size: " + QString::number(fileInfo.size()) + " bytes");

        // Check file magic bytes (first 16 bytes)
        QFile checkFile(baseImagePath);
        if (checkFile.open(QIODevice::ReadOnly)) {
            QByteArray header = checkFile.read(16);
            QString hexHeader;
            for (unsigned char byte : header) {
                hexHeader += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
            }
            LOG_MESSAGE("File header (hex): " + hexHeader);
            checkFile.close();

            // Check if it's a valid JPEG (starts with FF D8 FF)
            if (header.size() >= 3 &&
                (unsigned char)header[0] == 0xFF &&
                (unsigned char)header[1] == 0xD8 &&
                (unsigned char)header[2] == 0xFF) {
                LOG_MESSAGE("File has valid JPEG magic bytes");
            } else {
                LOG_MESSAGE("WARNING: File does NOT have valid JPEG magic bytes!");
            }
        }
    }

    // Log available image formats
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    QString formatsStr;
    for (const QByteArray& format : supportedFormats) {
        formatsStr += QString::fromLatin1(format) + " ";
    }
    LOG_MESSAGE("Qt supported image formats: " + formatsStr);

    QImage baseImage(baseImagePath);
    if (baseImage.isNull()) {
        LOG_MESSAGE("Error: QImage failed to load the file");

        // Try loading with explicit format hint
        LOG_MESSAGE("Attempting to load with explicit JPG format...");
        QImageReader reader(baseImagePath, "JPG");
        baseImage = reader.read();
        if (baseImage.isNull()) {
            LOG_MESSAGE("QImageReader also failed: " + reader.errorString());
        } else {
            LOG_MESSAGE("SUCCESS: QImageReader loaded the image!");
        }

        if (baseImage.isNull()) {
            LOG_MESSAGE("Checking for multi-page output...");

            // LibreOffice 可能生成了多页，尝试查找第一页
            QFileInfo info(baseImagePath);
            QString baseName = info.completeBaseName();
            QString firstPagePath = info.dir().absolutePath() + "/" + baseName + "_1.jpg";

            if (QFile::exists(firstPagePath)) {
                LOG_MESSAGE("Found multi-page output, using first page: " + firstPagePath);
                baseImage.load(firstPagePath);
                baseImagePath = firstPagePath;
            }
        }

        if (baseImage.isNull()) {
            LOG_MESSAGE("ERROR: Failed to load generated image - QImage.isNull() after all attempts");
            result.error = "Failed to load generated image";
            return result;
        }
    }

    // 5. 为每个关键词生成带水印的图片
    emit progress("生成水印图片...", 5, 5);
    QStringList generatedImages;
    QString baseName = QFileInfo(originalFileName).completeBaseName();

    int current = 0;
    for (const KeywordRule& rule : matchedRules) {
        current++;

        QImage watermarkedImage;
        QString outputFileName;

        if (rule.watermarkText.isEmpty()) {
            // 无水印原图
            watermarkedImage = baseImage;
            outputFileName = baseName + ".jpg";
        } else {
            // 添加水印
            watermarkedImage = addWatermark(baseImage, rule.watermarkText);
            // 使用检测关键词作为文件名前缀
            outputFileName = rule.detectText + "-" + baseName + ".jpg";
        }

        QString outputPath = outputDir + "/" + outputFileName;
        if (!watermarkedImage.save(outputPath, "JPG", 95)) {
            LOG_MESSAGE("Warning: Failed to save " + outputFileName);
            continue;
        }

        generatedImages.append(outputPath);
        result.generatedFiles.append(outputFileName);

        LOG_MESSAGE("Generated: " + outputFileName);
    }

    if (generatedImages.isEmpty()) {
        result.error = "No images generated";
        return result;
    }

    // 6. 根据配置决定是否打包为 ZIP
    if (config_.zipDownload) {
        LOG_MESSAGE("Creating ZIP file for " + QString::number(generatedImages.size()) + " images...");
        result.zipFilePath = createZipFile(generatedImages, outputDir, baseName + "_水印图片");
        LOG_MESSAGE("ZIP file path: " + result.zipFilePath);
        LOG_MESSAGE("ZIP file exists: " + QString(QFile::exists(result.zipFilePath) ? "Yes" : "No"));
        if (QFile::exists(result.zipFilePath)) {
            QFileInfo zipInfo(result.zipFilePath);
            LOG_MESSAGE("ZIP file size: " + QString::number(zipInfo.size()) + " bytes");
        }
    } else {
        LOG_MESSAGE("Zip download disabled, images will be downloaded individually");
        // 不打包，直接返回图片路径列表
        result.zipFilePath = "";  // 空字符串表示不使用ZIP
    }

    // 7. 清理临时文件
    QFile::remove(pdfPath);
    // Only remove baseImagePath if it's not one of the generated images
    // (when no watermark is added, the generated image IS the base image)
    if (!generatedImages.contains(baseImagePath)) {
        QFile::remove(baseImagePath);
    }
    if (config_.zipDownload) {
        // ZIP模式：打包后删除临时图片
        for (const QString& imagePath : generatedImages) {
            // QFile::remove(imagePath);  // 暂时保留，方便调试
        }
    }
    // 非ZIP模式：保留图片文件供下载

    result.success = true;
    emit progress("完成！", 5, 5);
    return result;
}

QString WatermarkService::convertWordToPdf(const QString& wordFilePath, const QString& outputDir) {
    LOG_MESSAGE("Converting Word to PDF using DocumentConverter...");

    // 使用 DocumentConverter 转换（会自动使用缓存）
    DocumentConverter::PreviewResult pdfResult = DocumentConverter::previewFile(wordFilePath);

    if (!pdfResult.success) {
        LOG_MESSAGE("Error: Failed to convert Word to PDF: " + pdfResult.error);
        return QString();
    }

    if (pdfResult.mimeType != "application/pdf") {
        LOG_MESSAGE("Error: Expected PDF output but got: " + pdfResult.mimeType);
        return QString();
    }

    // 保存 PDF 到临时目录
    QString pdfPath = outputDir + "/" + QFileInfo(wordFilePath).completeBaseName() + ".pdf";
    QFile pdfFile(pdfPath);
    if (!pdfFile.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE("Error: Failed to write PDF file: " + pdfPath);
        return QString();
    }

    pdfFile.write(pdfResult.data);
    pdfFile.close();

    LOG_MESSAGE("Successfully converted to PDF: " + pdfPath);
    return pdfPath;
}

QString WatermarkService::convertPdfToJpg(const QString& pdfFilePath, const QString& outputDir) {
    // 计算输出文件名
    QString baseName = QFileInfo(pdfFilePath).completeBaseName();
    QString jpgPath = outputDir + "/" + baseName + ".jpg";

    // 检查缓存：如果 JPG 已存在且比 PDF 新，直接返回
    if (QFile::exists(jpgPath)) {
        QFileInfo pdfInfo(pdfFilePath);
        QFileInfo jpgInfo(jpgPath);
        if (jpgInfo.lastModified() >= pdfInfo.lastModified()) {
            LOG_MESSAGE("Using cached JPG: " + jpgPath);
            return jpgPath;
        }
    }

    // 检查输入文件是否存在
    if (!QFile::exists(pdfFilePath)) {
        LOG_MESSAGE("Error: Input PDF file does not exist: " + pdfFilePath);
        return QString();
    }

    // 确保输出目录存在
    QDir dir(outputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_MESSAGE("Error: Failed to create output directory");
            return QString();
        }
    }

    // 使用 Ghostscript 转换（高质量、快速、字体正确）
    QStringList gsPaths = {
        "gswin64c.exe", "gswin32c.exe", "gs.exe",
        "C:/Program Files/gs/gs10.03.1/bin/gswin64c.exe",
        "C:/Program Files/gs/gs10.03.0/bin/gswin64c.exe",
        "C:/Program Files/gs/gs10.02.1/bin/gswin64c.exe",
        "C:/Program Files/gs/gs10.02.0/bin/gswin64c.exe",
        "C:/Program Files/gs/gs10.01.2/bin/gswin64c.exe",
        "C:/Program Files (x86)/gs/gs10.03.1/bin/gswin32c.exe",
        "C:/Program Files (x86)/gs/gs10.03.0/bin/gswin32c.exe"
    };

    QString foundGs;
    for (const QString& gsPath : gsPaths) {
        QProcess testProcess;
        configureProcessNoWindow(&testProcess);
        testProcess.start(gsPath, QStringList() << "--version");
        if (testProcess.waitForStarted(1000)) {
            testProcess.waitForFinished(3000);
            foundGs = gsPath;
            break;
        }
    }

    if (foundGs.isEmpty()) {
        LOG_MESSAGE("Error: Ghostscript not found. Please install Ghostscript from https://www.ghostscript.com/");
        emit logMessage("错误：未找到Ghostscript，请从 https://www.ghostscript.com/ 下载安装");
        return QString();
    }

    LOG_MESSAGE("Using Ghostscript: " + foundGs);

    QProcess gsProcess;
    configureProcessNoWindow(&gsProcess);

    QStringList args;
    args << "-dNOPAUSE" << "-dBATCH" << "-dSAFER"
         << "-sDEVICE=jpeg"
         << "-r300"  // 300 DPI 高质量
         << "-dJPEGQ=95"  // JPEG 质量 95%
         << QString("-sOutputFile=%1").arg(QDir::toNativeSeparators(jpgPath))
         << QDir::toNativeSeparators(pdfFilePath);

    LOG_MESSAGE("Ghostscript command: " + foundGs + " " + args.join(" "));

    gsProcess.start(foundGs, args);

    if (!gsProcess.waitForStarted(5000)) {
        LOG_MESSAGE("Error: Ghostscript failed to start");
        return QString();
    }

    if (!gsProcess.waitForFinished(15000)) {
        LOG_MESSAGE("Error: Ghostscript conversion timeout");
        gsProcess.kill();
        return QString();
    }

    if (gsProcess.exitCode() != 0) {
        QString stderr_output = QString::fromLocal8Bit(gsProcess.readAllStandardError());
        QString stdout_output = QString::fromLocal8Bit(gsProcess.readAllStandardOutput());
        LOG_MESSAGE("Error: Ghostscript conversion failed (exit code: " + QString::number(gsProcess.exitCode()) + ")");
        if (!stderr_output.isEmpty()) LOG_MESSAGE("stderr: " + stderr_output);
        if (!stdout_output.isEmpty()) LOG_MESSAGE("stdout: " + stdout_output);
        return QString();
    }

    // LibreOffice 可能生成多页，检查第一页
    if (!QFile::exists(jpgPath)) {
        QString firstPagePath = outputDir + "/" + baseName + "_1.jpg";
        if (QFile::exists(firstPagePath)) {
            jpgPath = firstPagePath;
            LOG_MESSAGE("Multi-page PDF detected, using first page");
        } else {
            LOG_MESSAGE("Error: JPG file not generated: " + jpgPath);
            return QString();
        }
    }

    LOG_MESSAGE("Successfully converted to JPG: " + jpgPath);
    return jpgPath;
}

QString WatermarkService::convertWordToHtml(const QString& wordFilePath, const QString& outputDir) {
    // 尝试多个可能的 LibreOffice 可执行文件名
    QStringList possibleCommands = {"soffice", "soffice.exe", "libreoffice", "libreoffice.exe"};
    QString sofficePath;

    // 检查哪个命令可用
    for (const QString& cmd : possibleCommands) {
        QProcess testProcess;
        configureProcessNoWindow(&testProcess);
        testProcess.start(cmd, QStringList() << "--version");
        if (testProcess.waitForStarted(1000)) {
            testProcess.waitForFinished(3000);
            sofficePath = cmd;
            break;
        }
    }

    if (sofficePath.isEmpty()) {
        emit logMessage("Error: LibreOffice not found. Please install LibreOffice and ensure it's in PATH.");
        emit logMessage("Tried commands: " + possibleCommands.join(", "));
        return QString();
    }

    QStringList args;
    args << "--headless"
         << "--convert-to" << "html:HTML:EmbedImages"
         << "--outdir" << outputDir
         << wordFilePath;

    emit logMessage("Converting Word to HTML using: " + sofficePath);
    emit logMessage("Input file: " + wordFilePath);
    emit logMessage("Output dir: " + outputDir);
    emit logMessage("Command: " + sofficePath + " " + args.join(" "));

    // 检查输入文件是否存在
    if (!QFile::exists(wordFilePath)) {
        emit logMessage("Error: Input Word file does not exist: " + wordFilePath);
        return QString();
    }

    // 确保输出目录存在
    QDir dir(outputDir);
    if (!dir.exists()) {
        emit logMessage("Creating output directory: " + outputDir);
        if (!dir.mkpath(".")) {
            emit logMessage("Error: Failed to create output directory");
            return QString();
        }
    }

    QProcess process;
    configureProcessNoWindow(&process);
    process.setWorkingDirectory(outputDir);
    process.start(sofficePath, args);

    if (!process.waitForStarted(5000)) {
        emit logMessage("Error: Failed to start LibreOffice (" + sofficePath + ")");
        return QString();
    }

    if (!process.waitForFinished(30000)) {
        emit logMessage("Error: LibreOffice conversion timeout");
        process.kill();
        return QString();
    }

    if (process.exitCode() != 0) {
        QString stderr_output = QString::fromLocal8Bit(process.readAllStandardError());
        QString stdout_output = QString::fromLocal8Bit(process.readAllStandardOutput());
        emit logMessage("Error: LibreOffice conversion failed (exit code: " + QString::number(process.exitCode()) + ")");
        if (!stderr_output.isEmpty()) emit logMessage("stderr: " + stderr_output);
        if (!stdout_output.isEmpty()) emit logMessage("stdout: " + stdout_output);
        return QString();
    }

    // 计算输出文件名
    QString baseName = QFileInfo(wordFilePath).completeBaseName();
    QString htmlPath = outputDir + "/" + baseName + ".html";

    if (!QFile::exists(htmlPath)) {
        emit logMessage("Error: HTML file not generated: " + htmlPath);
        return QString();
    }

    emit logMessage("Successfully converted to HTML: " + htmlPath);
    return htmlPath;
}

QString WatermarkService::convertDocumentToJpg(const QString& wordFilePath, const QString& outputDir) {
#ifdef Q_OS_WIN
    // Windows: 使用 Word COM API 转换
    QString pdfPath = convertWordToPdf(wordFilePath, outputDir);
    if (pdfPath.isEmpty()) {
        QString errorMsg = "错误：未检测到 Microsoft Word\n\n"
                          "请安装 Microsoft Word 以处理 Word 文档。\n"
                          "下载地址：https://www.microsoft.com/zh-cn/microsoft-365/word";
        LOG_MESSAGE(errorMsg);
        emit logMessage(errorMsg);
        return QString();
    }

    // 使用 Ghostscript 转 JPG
    QString jpgPath = convertPdfToJpg(pdfPath, outputDir);

    // 删除临时PDF
    if (!pdfPath.isEmpty() && QFile::exists(pdfPath)) {
        QFile::remove(pdfPath);
    }

    return jpgPath;
#else
    QString errorMsg = "错误：仅支持 Windows 系统\n\n"
                      "Word 文档处理需要 Microsoft Word，目前仅支持 Windows 系统。";
    LOG_MESSAGE(errorMsg);
    emit logMessage(errorMsg);
    return QString();
#endif
}

QString WatermarkService::extractOpinionText(const QString& htmlFilePath) {
    QFile file(htmlFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage("Error: Failed to open HTML file: " + htmlFilePath);
        return QString();
    }

    QXmlStreamReader xml(&file);
    int rowIndex = 0;
    int colIndex = 0;
    bool inTable = false;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            QString name = xml.name().toString().toLower();

            if (name == "table") {
                inTable = true;
                rowIndex = 0;
            } else if (inTable && name == "tr") {
                rowIndex++;
                colIndex = 0;
            } else if (inTable && (name == "td" || name == "th")) {
                colIndex++;

                if (rowIndex == 7 && colIndex == 2) {
                    QString text = xml.readElementText().trimmed();
                    file.close();
                    return text;
                }
            }
        } else if (xml.isEndElement()) {
            QString name = xml.name().toString().toLower();
            if (name == "table") {
                inTable = false;
            }
        }
    }

    file.close();

    if (xml.hasError()) {
        emit logMessage("Warning: XML parsing error: " + xml.errorString());
    }

    return QString();
}

QString WatermarkService::extractSuggestionFromWord(const QString& wordFilePath) {
    LOG_MESSAGE("Extracting suggestion from Word document: " + wordFilePath);

    QFile file(wordFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE("Error: Failed to open Word file: " + wordFilePath);
        return QString();
    }

    // 检测文件类型
    QByteArray header = file.read(512);
    file.seek(0);

    QString content;

    // WordML 格式（.doc 实际上是 XML）
    if (header.contains("<?xml") || header.contains("<w:wordDocument")) {
        LOG_MESSAGE("Detected WordML format");
        content = QString::fromUtf8(file.readAll());
        file.close();

        // 提取所有 <w:t> 标签内的文本，拼接为纯文本
        QString plainText;
        int searchPos = 0;
        while (true) {
            int tStart = content.indexOf("<w:t", searchPos);
            if (tStart == -1) break;
            int contentStart = content.indexOf(">", tStart);
            if (contentStart == -1) break;
            contentStart += 1;
            int contentEnd = content.indexOf("</w:t>", contentStart);
            if (contentEnd == -1) break;
            plainText += content.mid(contentStart, contentEnd - contentStart);
            searchPos = contentEnd + 6;
        }
        LOG_MESSAGE("Extracted plain text from WordML, length: " + QString::number(plainText.size()));

        // 搜索 "拟办意见" 或 "建议："
        int suggestionIndex = plainText.indexOf("拟办意见");
        if (suggestionIndex == -1) {
            suggestionIndex = plainText.indexOf("建议：");
        }
        if (suggestionIndex == -1) {
            LOG_MESSAGE("Warning: No '拟办意见' or '建议：' found in WordML document");
            return QString();
        }

        int startPos = suggestionIndex;
        int endPos = plainText.size();
        if (endPos - startPos > 500) {
            endPos = startPos + 500;
        }

        QString suggestion = plainText.mid(startPos, endPos - startPos).trimmed();
        LOG_MESSAGE("Extracted from WordML: " + suggestion);
        return suggestion;
    }

    // docx 格式（ZIP 压缩包）
    if (header.startsWith("PK")) {
        LOG_MESSAGE("Detected docx format");
        file.close();

        // 使用临时目录解压
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            LOG_MESSAGE("Error: Failed to create temp directory");
            return QString();
        }

        // 解压 docx 文件
        QProcess unzip;
        configureProcessNoWindow(&unzip);
        #ifdef Q_OS_WIN
        // Windows: PowerShell Expand-Archive 只支持 .zip 扩展名
        // 创建临时 .zip 副本
        QString zipPath = wordFilePath + ".zip";
        if (!QFile::copy(wordFilePath, zipPath)) {
            LOG_MESSAGE("Error: Failed to create .zip copy");
            return QString();
        }

        QStringList args;
        args << "-WindowStyle" << "Hidden"
             << "-Command"
             << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                .arg(QDir::toNativeSeparators(zipPath), QDir::toNativeSeparators(tempDir.path()));
        LOG_MESSAGE("Extracting docx with PowerShell: " + args.join(" "));
        unzip.start("powershell", args);
        #else
        // Linux/Mac: 使用 unzip
        unzip.start("unzip", QStringList() << "-q" << "-o" << wordFilePath << "-d" << tempDir.path());
        #endif

        if (!unzip.waitForStarted(5000)) {
            LOG_MESSAGE("Error: Failed to start unzip process");
            #ifdef Q_OS_WIN
            QFile::remove(zipPath);
            #endif
            return QString();
        }

        if (!unzip.waitForFinished(10000)) {
            LOG_MESSAGE("Error: Unzip timeout");
            unzip.kill();
            #ifdef Q_OS_WIN
            QFile::remove(zipPath);
            #endif
            return QString();
        }

        #ifdef Q_OS_WIN
        // 清理临时 .zip 文件
        QFile::remove(zipPath);
        #endif

        if (unzip.exitCode() != 0) {
            LOG_MESSAGE("Error: Unzip failed with exit code " + QString::number(unzip.exitCode()));
            QString errorOutput = QString::fromLocal8Bit(unzip.readAllStandardError());
            if (!errorOutput.isEmpty()) {
                LOG_MESSAGE("Unzip error: " + errorOutput);
            }
            return QString();
        }

        LOG_MESSAGE("Successfully extracted docx file");

        QString xmlPath = tempDir.path() + "/word/document.xml";
        QFile xmlFile(xmlPath);
        if (!xmlFile.open(QIODevice::ReadOnly)) {
            LOG_MESSAGE("Error: Failed to open document.xml from docx: " + xmlPath);
            LOG_MESSAGE("Checking if file exists: " + QString(QFile::exists(xmlPath) ? "Yes" : "No"));
            return QString();
        }

        content = QString::fromUtf8(xmlFile.readAll());
        xmlFile.close();
        LOG_MESSAGE("Read document.xml, size: " + QString::number(content.size()) + " chars");

        // 提取所有 <w:t> 标签内的文本，拼接为纯文本
        QString plainText;
        int searchPos = 0;
        while (true) {
            int tStart = content.indexOf("<w:t", searchPos);
            if (tStart == -1) break;
            int contentStart = content.indexOf(">", tStart);
            if (contentStart == -1) break;
            contentStart += 1;
            int contentEnd = content.indexOf("</w:t>", contentStart);
            if (contentEnd == -1) break;
            plainText += content.mid(contentStart, contentEnd - contentStart);
            searchPos = contentEnd + 6;
        }
        LOG_MESSAGE("Extracted plain text from docx, length: " + QString::number(plainText.size()));

        // 搜索 "拟办意见" 或 "建议："
        int suggestionIndex = plainText.indexOf("拟办意见");
        if (suggestionIndex == -1) {
            suggestionIndex = plainText.indexOf("建议：");
        }
        if (suggestionIndex == -1) {
            LOG_MESSAGE("Warning: No '拟办意见' or '建议：' found in docx document");
            return QString();
        }

        // 提取标签后的内容（最多500字符）
        int startPos = suggestionIndex;
        int endPos = plainText.size();
        if (endPos - startPos > 500) {
            endPos = startPos + 500;
        }

        QString suggestion = plainText.mid(startPos, endPos - startPos).trimmed();
        LOG_MESSAGE("Extracted from docx: " + suggestion);
        return suggestion;
    }

    file.close();
    LOG_MESSAGE("Error: Unknown Word document format");
    return QString();
}

QList<WatermarkService::KeywordRule> WatermarkService::matchKeywords(const QString& text) {
    QList<KeywordRule> matched;

    // 先收集所有匹配的规则
    QList<KeywordRule> allMatched;
    for (const KeywordRule& rule : keywords_) {
        if (!rule.enabled) {
            continue;
        }

        // 精确匹配整个短语
        if (text.contains(rule.detectText)) {
            allMatched.append(rule);
        }
    }

    // 如果没有匹配，直接返回
    if (allMatched.isEmpty()) {
        return matched;
    }

    // 按检测文本长度降序排序（长的优先）
    std::sort(allMatched.begin(), allMatched.end(), [](const KeywordRule& a, const KeywordRule& b) {
        return a.detectText.length() > b.detectText.length();
    });

    // 去重：如果一个关键词是另一个已选中关键词的子串，则跳过
    for (const KeywordRule& rule : allMatched) {
        bool isSubstring = false;
        for (const KeywordRule& selectedRule : matched) {
            if (selectedRule.detectText.contains(rule.detectText) &&
                selectedRule.detectText != rule.detectText) {
                isSubstring = true;
                break;
            }
        }

        if (!isSubstring) {
            matched.append(rule);
            LOG_MESSAGE("Matched keyword: " + rule.detectText + " -> " + rule.watermarkText);
        }
    }

    return matched;
}

QImage WatermarkService::addWatermark(const QImage& source, const QString& text) {
    QImage result = source.copy();
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 设置字体
    QFont font("SimSun", config_.fontSize);
    painter.setFont(font);

    // 设置颜色和透明度
    QColor color = config_.color;
    color.setAlpha(255 * config_.opacity / 100);
    painter.setPen(color);

    // 计算文字尺寸
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(text);

    // 根据密度计算间距
    int baseSpacing = qMax(textRect.width(), textRect.height()) * 3;  // 增加基础间距
    int spacing = baseSpacing;
    if (config_.density == "sparse") {
        spacing = baseSpacing * 2.5;  // 稀疏：更大间距
    } else if (config_.density == "dense") {
        spacing = baseSpacing * 0.8;  // 密集：较小间距
    } else {  // medium
        spacing = baseSpacing * 1.5;  // 中等
    }

    // 保存当前状态
    painter.save();

    // 计算旋转后需要绘制的范围
    int diagonal = static_cast<int>(std::sqrt(result.width() * result.width() + result.height() * result.height()));

    // 平移到中心，旋转
    painter.translate(result.width() / 2, result.height() / 2);
    painter.rotate(config_.rotation);
    painter.translate(-result.width() / 2, -result.height() / 2);

    // 满铺绘制水印
    for (int y = -diagonal; y < result.height() + diagonal; y += spacing) {
        for (int x = -diagonal; x < result.width() + diagonal; x += spacing) {
            painter.drawText(x, y, text);
        }
    }

    painter.restore();
    painter.end();

    return result;
}

QString WatermarkService::createZipFile(const QStringList& imagePaths, const QString& outputDir, const QString& baseName) {
    if (imagePaths.isEmpty()) {
        emit logMessage("Error: No images to zip");
        return QString();
    }

    QString zipPath = outputDir + "/" + baseName + ".zip";

    // 获取文件名列表
    QStringList fileNames;
    for (const QString& path : imagePaths) {
        fileNames.append(QFileInfo(path).fileName());
    }

    QProcess process;
    configureProcessNoWindow(&process);
    process.setWorkingDirectory(outputDir);

    #ifdef Q_OS_WIN
    // Windows: 使用 PowerShell Compress-Archive
    QString filesArg = fileNames.join("','");
    QStringList args;
    args << "-WindowStyle" << "Hidden"
         << "-Command"
         << QString("Compress-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(filesArg, QFileInfo(zipPath).fileName());

    emit logMessage("Creating ZIP with PowerShell: " + args.join(" "));
    process.start("powershell", args);
    #else
    // Linux/Mac: 使用 zip
    QStringList args;
    args << zipPath;
    args.append(fileNames);
    process.start("zip", args);
    #endif

    if (!process.waitForFinished(10000)) {
        emit logMessage("Warning: ZIP creation timeout");
        process.kill();
        // Fallback: if only one file, return it directly
        if (imagePaths.size() == 1) {
            return imagePaths.first();
        }
        return QString();
    }

    if (process.exitCode() != 0) {
        emit logMessage("Warning: ZIP creation failed with exit code " + QString::number(process.exitCode()));
        QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
        if (!errorOutput.isEmpty()) {
            emit logMessage("ZIP error output: " + errorOutput);
        }
    }

    if (QFile::exists(zipPath)) {
        emit logMessage("ZIP file created successfully: " + zipPath);
        return zipPath;
    }

    emit logMessage("Warning: ZIP file does not exist after creation attempt");
    // Fallback: if only one file, return it directly
    if (imagePaths.size() == 1) {
        emit logMessage("Returning single image file instead: " + imagePaths.first());
        return imagePaths.first();
    }

    return QString();
}

}
