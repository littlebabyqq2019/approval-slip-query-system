#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMutex>

namespace CrossNetShare {

struct ApprovalRecord {
    QString id;                  // 数据库主键 ID
    QString receiveNumber;       // 收文编号 A-26-1（左侧列表显示）
    QString department;          // 来文单位
    QString wordCode;            // 来文字号
    QString fileReceiveDate;     // 收文日期
    QString fileCategory;        // 来文类型
    QString receiveChannel;      // 收文途径
    QString emergencyLevel;      // 紧急程度
    QString secretLevel;         // 密级
    QString summary;             // 文件标题
    QString leaderInstruction;   // 领导批示
    QString suggestion;          // 拟办意见
    QString processResult;       // 办理结果
    QString organizer;           // 承办
    QString notes;               // 备注
    QString createTime;          // 创建时间
    QString modifyTime;          // 修改时间

    // 原始 JSON 字符串（供生成文档用）
    QString rawJson;
};

class DbManager : public QObject {
    Q_OBJECT
public:
    explicit DbManager(QObject* parent = nullptr);
    ~DbManager();

    // 设置 H2 数据库文件基础路径（不包含 .mv.db 后缀）
    // 例如: e:/data/doc-2026 会匹配 e:/data/doc-2026.mv.db
    bool setDatabasePath(const QString& dbBasePath);

    QString getDatabasePath() const { return dbBasePath_; }
    bool isDatabaseLoaded() const { return !dbBasePath_.isEmpty(); }

    // 刷新并重新读取所有记录
    bool refresh();

    // 获取所有记录（含收文编号）
    QList<ApprovalRecord> getAllRecords() const;

    // 按关键字搜索
    QList<ApprovalRecord> searchRecords(const QString& keyword) const;

    // 按 ID 取单条记录
    bool getRecordById(const QString& id, ApprovalRecord& outRecord) const;

    // 根据记录生成填充好的 Word 和 PDF 文件
    // 返回生成的 PDF 路径；失败返回空字符串
    // outputPdfPath 和 outputDocxPath 用于输出实际文件路径
    QString generateDocument(const ApprovalRecord& record,
                             const QString& templatePath,
                             QString& outputDocxPath,
                             QString& outputPdfPath,
                             QString& errorMessage);

    // 根据记录生成 HTML 预览（使用 Aspose.Words 转换，保留模板样式）
    QString generatePreviewHtml(const ApprovalRecord& record,
                                const QString& templatePath,
                                QString& errorMessage);

    static DbManager* instance();

signals:
    void databaseChanged();
    void error(const QString& msg);

private:
    QString dbBasePath_;
    mutable QMutex mutex_;
    QList<ApprovalRecord> records_;

    QString findPython() const;
    QString findAppDir() const;
    QMap<QString, QString> runPythonJson(const QStringList& args, QString& error) const;
    bool parseRecordsJson(const QString& jsonText, QList<ApprovalRecord>& outRecords) const;
};

}
