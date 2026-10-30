#pragma once

#include "../server.h"
#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QFileDialog>

namespace CrossNetShare {

class WatermarkService;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // Public method for redirecting qDebug output
    void appendDebugLog(const QString& message);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStartStopClicked();
    void onSelectDatabaseClicked();
    void onAddRemoteDatabaseClicked();
    void onRefreshDbClicked();
    void onServerStarted();
    void onServerStopped();
    void onLogMessage(const QString& message);
    void onServerError(const QString& errorMsg);
    void onCleanupCache();
    void onSettingsClicked();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onQuitApp();
    void onDbManagerError(const QString& msg);
    void onDbDatabaseChanged();
    void saveDatabaseConfig();
    void loadDatabaseConfig();
    void onDbListItemChanged(QListWidgetItem* item);
    void onAutoRefreshDatabase();

private:
    void setupUi();
    void setupTrayIcon();
    void updateServerStatus();
    void appendLog(const QString& message);
    void updateDbListWidget();
    QString databaseConfigFile() const;  // 配置文件路径：优先程序目录（可写时），否则用户 AppData

    Server* server_;
    WatermarkService* watermarkService_;
    QTimer* cacheCleanupTimer_;
    QTimer* dbRefreshTimer_;  // 数据库自动刷新定时器

    // UI控件
    QSpinBox* portSpinBox_;
    QSpinBox* webPortSpinBox_;
    QPushButton* startStopButton_;
    QPushButton* selectDbButton_;
    QPushButton* addRemoteDbButton_;
    QPushButton* refreshDbButton_;
    QLineEdit* dbPathLineEdit_;
    QLabel* dbStatusLabel_;
    QLabel* statusLabel_;
    QTextEdit* logTextEdit_;
    QListWidget* dbListWidget_;  // 数据库列表

    // 系统托盘
    QSystemTrayIcon* trayIcon_;
    QMenu* trayMenu_;

    // 启动恢复配置期间为 true：此时不写 config.json，
    // 否则 addDatabase() 内部触发的 databaseChanged 会用"只加了一半"的列表覆盖配置文件
    bool loadingConfig_ = false;
};

}
