#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QString>
#include <QByteArray>
#include <QPointer>

namespace CrossNetShare {

class FileIndexer;
class AuthManager;
class Server;
class WatermarkService;
enum UserPermissionFlag : unsigned int;  // 前向声明枚举类型

struct HttpRequest {
    QString method;         // GET, POST, etc.
    QString path;           // /api/login
    QString version;        // HTTP/1.1
    QMap<QString, QString> headers;
    QMap<QString, QString> params;  // Query parameters
    QByteArray body;
};

struct HttpResponse {
    int statusCode;
    QString statusText;
    QMap<QString, QString> headers;
    QByteArray body;

    HttpResponse() : statusCode(200), statusText("OK") {}
};

class WebServer : public QObject {
    Q_OBJECT

public:
    explicit WebServer(FileIndexer* indexer, QObject* parent = nullptr);
    ~WebServer();

    bool start(quint16 port = 8080);
    void stop();

    bool isRunning() const { return running_; }
    quint16 getPort() const { return port_; }

    void setAuthManager(AuthManager* authManager);
    void setServer(Server* server);
    void setWatermarkService(WatermarkService* watermarkService);

signals:
    void started();
    void stopped();
    void logMessage(const QString& message);
    void error(const QString& errorMsg);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    bool parseHttpRequest(const QByteArray& data, HttpRequest& request);
    void handleRequest(QTcpSocket* socket, const HttpRequest& request);
    void sendResponse(QTcpSocket* socket, const HttpResponse& response);

    // API handlers
    void handleLogin(QTcpSocket* socket, const HttpRequest& request);
    void handleLogout(QTcpSocket* socket, const HttpRequest& request);
    void handleUserInfo(QTcpSocket* socket, const HttpRequest& request);
    void handleDatabaseList(QTcpSocket* socket, const HttpRequest& request);
    void handleDatabaseSelect(QTcpSocket* socket, const HttpRequest& request);
    void handleFileList(QTcpSocket* socket, const HttpRequest& request);
    void handleFileSearch(QTcpSocket* socket, const HttpRequest& request);
    void handleFileDownload(QTcpSocket* socket, const HttpRequest& request);
    void handleBatchDownload(QTcpSocket* socket, const HttpRequest& request);
    void handleFilePreview(QTcpSocket* socket, const HttpRequest& request);
    void handleWatermarkGenerate(QTcpSocket* socket, const HttpRequest& request);
    void handleWatermarkDownload(QTcpSocket* socket, const HttpRequest& request);
    void handleContentSearch(QTcpSocket* socket, const HttpRequest& request);

    // Static file serving
    void serveStaticFile(QTcpSocket* socket, const QString& path);
    void serveLoginPage(QTcpSocket* socket);
    void serveBrowsePage(QTcpSocket* socket);

    // Helper functions
    QString getMimeType(const QString& filename);
    bool isAuthenticated(const HttpRequest& request, QString& username);
    bool hasPermission(const QString& username, UserPermissionFlag permission);
    QByteArray buildHttpResponse(const HttpResponse& response);

    // Batch download helper
    struct FileInfo {
        QString tempPath;
        QString fileName;
    };
    void processBatchDownloadFiles(QTcpSocket* socket, const QList<FileInfo>& files);

    // Watermark helper
    void processWatermarkGeneration(QTcpSocket* socket, const QString& tempDir, const QString& tempFilePath, const QString& username, const QString& suggestionText = "");

    QTcpServer* tcpServer_;
    FileIndexer* indexer_;
    AuthManager* authManager_;
    Server* server_;
    WatermarkService* watermarkService_;
    bool running_;
    quint16 port_;

    QMap<QTcpSocket*, QByteArray> socketBuffers_;

    // 水印会话存储（sessionId -> {tempDir, fileList}）
    struct WatermarkSession {
        QString tempDir;
        QStringList fileNames;
    };
    QMap<QString, WatermarkSession> watermarkSessions_;
};

}
