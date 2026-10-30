#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMutex>
#include <QThread>
#include <atomic>

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
    // 仅登记路径到列表（不建连），用于启动时恢复配置：避免启动阶段串行建连远程库导致界面卡死
    void registerDatabasePath(const QString& dbBasePath);
    bool removeDatabase(const QString& dbBasePath);
    QStringList getDatabaseList() const;
    bool setActiveDatabase(const QString& dbBasePath);
    QString getActiveDatabase() const;

    QString getDatabasePath() const { return activeDbPath_; }
    bool isDatabaseLoaded() const { return !activeDbPath_.isEmpty(); }

    bool refresh();

    // 异步刷新（后台线程执行，立即返回）。用于远程库实时更新：
    // refresh() 内部要跑 Python+Java 子进程，耗时数秒，
    // Web 服务是单线程事件循环，同步调用会把整个服务卡住。
    void refreshAsync();

    // 节流请求刷新：数据超过 maxAgeMs 未刷新时，触发一次后台刷新。
    // 非阻塞，调用方应立即用 getAllRecords() 返回当前缓存。
    // 返回 true 表示本次已触发后台刷新。
    bool requestRefreshIfStale(int maxAgeMs);

    // 当前活动库是否为远程库（tcp:// 前缀）
    bool isRemoteActive() const;

    // 是否已有后台刷新在执行
    bool isRefreshing() const;

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

    // 上次成功刷新时间（毫秒时间戳），用于 refreshIfStale 节流
    mutable qint64 lastRefreshMs_ = 0;

    // 后台刷新线程控制
    std::atomic<int> refreshing_{0};       // 0=空闲 1=执行中
    std::atomic<int> refreshRequested_{0}; // 0=无待处理请求 1=有待处理
    void refreshWorker();

    QString findPython() const;
    QString findAppDir() const;
    QMap<QString, QString> runPythonJson(const QStringList& args, QString& error) const;
    bool parseRecordsJson(const QString& jsonText, QList<ApprovalRecord>& outRecords) const;
};

}
