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
    bool setDatabasePath(const QString& dbBasePath);

    // 多数据库支持
    bool addDatabase(const QString& dbBasePath);
    bool removeDatabase(const QString& dbBasePath);
    QStringList getDatabaseList() const;
    bool setActiveDatabase(const QString& dbBasePath);
    QString getActiveDatabase() const;

    QString getDatabasePath() const { return activeDbPath_; }
    bool isDatabaseLoaded() const { return !activeDbPath_.isEmpty(); }

    bool refresh();

    QList<ApprovalRecord> getAllRecords() const;
    QList<ApprovalRecord> searchRecords(const QString& keyword) const;
    bool getRecordById(const QString& id, ApprovalRecord& outRecord) const;

    QString generateDocument(const ApprovalRecord& record,
                             const QString& templatePath,
                             QString& outputDocxPath,
                             QString& outputPdfPath,
                             QString& errorMessage);

    static DbManager* instance();

signals:
    void databaseChanged();
    void databaseListChanged();
    void error(const QString& msg);

private:
    QString activeDbPath_;
    mutable QMutex mutex_;
    QList<ApprovalRecord> records_;

    // Multi-database: path → display name
    QStringList databasePaths_;

    QString findPython() const;
    QString findAppDir() const;
    QMap<QString, QString> runPythonJson(const QStringList& args, QString& error) const;
    bool parseRecordsJson(const QString& jsonText, QList<ApprovalRecord>& outRecords) const;
};

}
