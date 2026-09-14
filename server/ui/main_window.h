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

private:
    void setupUi();
    void setupTrayIcon();
    void updateServerStatus();
    void appendLog(const QString& message);

    Server* server_;
    WatermarkService* watermarkService_;
    QTimer* cacheCleanupTimer_;

    // UI控件
    QSpinBox* portSpinBox_;
    QSpinBox* webPortSpinBox_;
    QPushButton* startStopButton_;
    QPushButton* selectDbButton_;
    QPushButton* refreshDbButton_;
    QLineEdit* dbPathLineEdit_;
    QLabel* dbStatusLabel_;
    QLabel* statusLabel_;
    QTextEdit* logTextEdit_;

    // 系统托盘
    QSystemTrayIcon* trayIcon_;
    QMenu* trayMenu_;
};

}
