#include "web_server.h"
#include "server.h"
#include "client_handler.h"
#include "auth_manager.h"
#include "user_manager.h"
#include "document_converter.h"
#include "watermark_service.h"
#include "file_indexer.h"
#include "db_manager.h"
#include "audit_logger.h"
#include "common/protocol.h"
#include "common/version.h"
#include <nlohmann/json.hpp>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QTextStream>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace CrossNetShare {

namespace {
QString urlDecode(const QString& value) {
    return QUrl::fromPercentEncoding(value.toUtf8());
}

QString cookieValue(const HttpRequest& request, const QString& name) {
    QString cookie = request.headers.value("cookie");
    const QStringList parts = cookie.split(';', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        QStringList kv = part.trimmed().split('=');
        if (kv.size() == 2 && kv[0] == name) {
            return kv[1];
        }
    }
    return QString();
}

QByteArray jsonResponse(const nlohmann::json& json) {
    return QByteArray::fromStdString(json.dump());
}

QString fileId(const FileMetadata& file) {
    return QUrl::toPercentEncoding(QString::fromStdString(file.ownerClient)) + "|" +
           QUrl::toPercentEncoding(QString::fromStdString(file.relativePath));
}

bool parseFileId(const QString& id, QString& clientId, QString& relativePath) {
    const QStringList parts = id.split('|');
    if (parts.size() != 2) {
        return false;
    }
    clientId = QUrl::fromPercentEncoding(parts[0].toUtf8());
    relativePath = QUrl::fromPercentEncoding(parts[1].toUtf8());
    return !clientId.isEmpty() && !relativePath.isEmpty();
}

QString humanSize(quint64 size) {
    if (size < 1024) return QString::number(size) + " B";
    if (size < 1024 * 1024) return QString::number(size / 1024.0, 'f', 1) + " KB";
    if (size < 1024ULL * 1024ULL * 1024ULL) return QString::number(size / 1024.0 / 1024.0, 'f', 1) + " MB";
    return QString::number(size / 1024.0 / 1024.0 / 1024.0, 'f', 1) + " GB";
}
}

WebServer::WebServer(FileIndexer* indexer, QObject* parent)
    : QObject(parent)
    , tcpServer_(new QTcpServer(this))
    , indexer_(indexer)
    , authManager_(nullptr)
    , server_(nullptr)
    , watermarkService_(nullptr)
    , running_(false)
    , port_(8080)
{
    connect(tcpServer_, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start(quint16 port) {
    if (running_) {
        return true;
    }

    port_ = port;
    if (!tcpServer_->listen(QHostAddress::Any, port_)) {
        emit error("Web server failed to listen on port " + QString::number(port_) + ": " + tcpServer_->errorString());
        return false;
    }

    running_ = true;
    emit started();
    emit logMessage("Web server listening on http://0.0.0.0:" + QString::number(port_));
    return true;
}

void WebServer::stop() {
    if (!running_) {
        return;
    }

    tcpServer_->close();
    for (QTcpSocket* socket : socketBuffers_.keys()) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    socketBuffers_.clear();

    running_ = false;
    emit stopped();
    emit logMessage("Web server stopped");
}

void WebServer::setAuthManager(AuthManager* authManager) {
    authManager_ = authManager;
}

void WebServer::setServer(Server* server) {
    server_ = server;
}

void WebServer::setWatermarkService(WatermarkService* watermarkService) {
    watermarkService_ = watermarkService;
}

void WebServer::onNewConnection() {
    while (tcpServer_->hasPendingConnections()) {
        QTcpSocket* socket = tcpServer_->nextPendingConnection();
        socketBuffers_[socket] = QByteArray();
        connect(socket, &QTcpSocket::readyRead, this, &WebServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebServer::onDisconnected);
    }
}

void WebServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    socketBuffers_[socket].append(socket->readAll());
    HttpRequest request;
    if (!parseHttpRequest(socketBuffers_[socket], request)) {
        return;
    }

    socketBuffers_[socket].clear();
    handleRequest(socket, request);
}

void WebServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socketBuffers_.remove(socket);
        socket->deleteLater();
    }
}

bool WebServer::parseHttpRequest(const QByteArray& data, HttpRequest& request) {
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return false;
    }

    QByteArray headerData = data.left(headerEnd);
    QList<QByteArray> lines = headerData.split('\n');
    if (lines.isEmpty()) {
        return false;
    }

    QList<QByteArray> requestLine = lines[0].trimmed().split(' ');
    if (requestLine.size() < 3) {
        return false;
    }

    request.method = QString::fromLatin1(requestLine[0]).toUpper();
    QString rawUrl = QString::fromUtf8(requestLine[1]);
    request.version = QString::fromLatin1(requestLine[2]);

    QUrl url(rawUrl);
    request.path = url.path();
    QUrlQuery query(url);
    const auto items = query.queryItems(QUrl::FullyDecoded);
    for (const auto& item : items) {
        request.params[item.first] = item.second;
    }

    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i].trimmed();
        int colon = line.indexOf(':');
        if (colon > 0) {
            QString key = QString::fromLatin1(line.left(colon)).toLower();
            QString value = QString::fromUtf8(line.mid(colon + 1).trimmed());
            request.headers[key] = value;
        }
    }

    int contentLength = request.headers.value("content-length").toInt();
    if (data.size() < headerEnd + 4 + contentLength) {
        return false;
    }

    request.body = data.mid(headerEnd + 4, contentLength);
    return true;
}

void WebServer::handleRequest(QTcpSocket* socket, const HttpRequest& request) {
    // Log all incoming requests
    qDebug() << "[HTTP]" << request.method << request.path;

    if (request.path == "/" || request.path == "/login") {
        serveLoginPage(socket);
        return;
    }
    if (request.path == "/browse") {
        QString username;
        if (!isAuthenticated(request, username)) {
            HttpResponse response;
            response.statusCode = 302;
            response.statusText = "Found";
            response.headers["Location"] = "/login";
            sendResponse(socket, response);
            return;
        }
        serveBrowsePage(socket);
        return;
    }
    if (request.path == "/api/login") {
        handleLogin(socket, request);
        return;
    }
    if (request.path == "/api/logout") {
        handleLogout(socket, request);
        return;
    }

    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "Unauthorized"}});
        sendResponse(socket, response);
        return;
    }

    if (request.path == "/api/user") {
        handleUserInfo(socket, request);
    } else if (request.path == "/api/databases") {
        handleDatabaseList(socket, request);
    } else if (request.path == "/api/databases/select") {
        handleDatabaseSelect(socket, request);
    } else if (request.path == "/api/files") {
        handleFileList(socket, request);
    } else if (request.path == "/api/search") {
        handleFileSearch(socket, request);
    } else if (request.path == "/api/content-search") {
        handleContentSearch(socket, request);
    } else if (request.path == "/api/download") {
        handleFileDownload(socket, request);
    } else if (request.path == "/api/batch-download") {
        handleBatchDownload(socket, request);
    } else if (request.path == "/api/preview") {
        handleFilePreview(socket, request);
    } else if (request.path == "/api/watermark/generate") {
        qDebug() << "[Watermark] Route matched: /api/watermark/generate";
        handleWatermarkGenerate(socket, request);
    } else if (request.path.startsWith("/api/watermark/download")) {
        handleWatermarkDownload(socket, request);
    } else {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "text/plain; charset=utf-8";
        response.body = "Not Found";
        sendResponse(socket, response);
    }
}

void WebServer::sendResponse(QTcpSocket* socket, const HttpResponse& response) {
    socket->write(buildHttpResponse(response));
    socket->flush();
    socket->disconnectFromHost();
}

void WebServer::handleLogin(QTcpSocket* socket, const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";

    if (!authManager_) {
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.body = jsonResponse({{"success", false}, {"error", "Authentication is not configured"}});
        sendResponse(socket, response);
        return;
    }

    QString username;
    QString password;
    try {
        auto body = nlohmann::json::parse(request.body.constData(), request.body.constData() + request.body.size());
        username = QString::fromStdString(body.value("username", ""));
        password = QString::fromStdString(body.value("password", ""));
    } catch (...) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = jsonResponse({{"success", false}, {"error", "Invalid JSON"}});
        sendResponse(socket, response);
        return;
    }

    QString token = authManager_->authenticate(username, password);
    QString clientIp = socket->peerAddress().toString();

    if (token.isEmpty()) {
        // 记录登录失败
        AuditLogger::instance()->log(username, AuditAction::Login, "", clientIp, false);

        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.body = jsonResponse({{"success", false}, {"error", "Invalid username or password"}});
        sendResponse(socket, response);
        return;
    }

    // 记录登录成功
    AuditLogger::instance()->log(username, AuditAction::Login, "", clientIp, true);

    response.headers["Set-Cookie"] = "CNS_SESSION=" + token + "; Path=/; HttpOnly; SameSite=Strict";
    response.body = jsonResponse({{"success", true}, {"redirect", "/browse"}});
    sendResponse(socket, response);
}

void WebServer::handleLogout(QTcpSocket* socket, const HttpRequest& request) {
    QString username;
    QString token = cookieValue(request, "CNS_SESSION");

    if (authManager_ && isAuthenticated(request, username)) {
        // 记录登出
        AuditLogger::instance()->log(username, AuditAction::Logout, "", socket->peerAddress().toString(), true);
        authManager_->logout(token);
    }

    HttpResponse response;
    response.statusCode = 302;
    response.statusText = "Found";
    response.headers["Location"] = "/login";
    response.headers["Set-Cookie"] = "CNS_SESSION=deleted; Path=/; Max-Age=0";
    sendResponse(socket, response);
}

void WebServer::handleUserInfo(QTcpSocket* socket, const HttpRequest& request) {
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "Unauthorized"}});
        sendResponse(socket, response);
        return;
    }

    // 获取用户信息
    User user = UserManager::instance()->getUser(username);

    nlohmann::json userJson;
    userJson["username"] = username.toStdString();
    userJson["isAdmin"] = user.isAdmin;
    userJson["permissions"] = static_cast<int>(user.permissions);  // 权限位标志
    userJson["permissionsText"] = permissionsToString(user.permissions).toStdString();

    // 各个权限项（方便前端使用）
    nlohmann::json perms;
    perms["viewFileList"] = user.permissions.testFlag(UserPermissionFlag::ViewFileList);
    perms["previewFile"] = user.permissions.testFlag(UserPermissionFlag::PreviewFile);
    perms["printFile"] = user.permissions.testFlag(UserPermissionFlag::PrintFile);
    perms["downloadFile"] = user.permissions.testFlag(UserPermissionFlag::DownloadFile);
    perms["batchDownload"] = user.permissions.testFlag(UserPermissionFlag::BatchDownload);
    perms["watermarkExport"] = user.permissions.testFlag(UserPermissionFlag::WatermarkExport);
    perms["dateFilter"] = user.permissions.testFlag(FilterByDate);
    userJson["permissionFlags"] = perms;

    nlohmann::json responseJson;
    responseJson["success"] = true;
    responseJson["user"] = userJson;
    responseJson["watermarkEnabled"] = (watermarkService_ && watermarkService_->getConfig().enabled);

    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = QByteArray::fromStdString(responseJson.dump());
    sendResponse(socket, response);
}

void WebServer::handleDatabaseList(QTcpSocket* socket, const HttpRequest& request) {
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.body = jsonResponse({{"success", false}, {"error", "未登录"}});
        sendResponse(socket, response);
        return;
    }
    QStringList dbs = DbManager::instance()->getDatabaseList();
    QString active = DbManager::instance()->getActiveDatabase();
    nlohmann::json root;
    root["success"] = true;
    root["databases"] = nlohmann::json::array();
    for (const QString& db : dbs) {
        nlohmann::json item;
        QFileInfo fi(db + ".mv.db");
        item["path"] = db.toStdString();
        item["name"] = fi.fileName().toStdString();
        item["active"] = (db == active);
        root["databases"].push_back(item);
    }
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = jsonResponse(root);
    sendResponse(socket, response);
}

void WebServer::handleDatabaseSelect(QTcpSocket* socket, const HttpRequest& request) {
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.body = jsonResponse({{"success", false}, {"error", "未登录"}});
        sendResponse(socket, response);
        return;
    }
    QString dbPath = request.params.value("path");
    if (dbPath.isEmpty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = jsonResponse({{"success", false}, {"error", "缺少path参数"}});
        sendResponse(socket, response);
        return;
    }
    bool ok = DbManager::instance()->setActiveDatabase(dbPath);
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = jsonResponse({{"success", ok}});
    sendResponse(socket, response);
}

void WebServer::handleFileList(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证和权限
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "未登录"}});
        sendResponse(socket, response);
        return;
    }

    if (!hasPermission(username, UserPermissionFlag::ViewFileList)) {
        HttpResponse response;
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "无权限浏览文件列表"}});
        sendResponse(socket, response);
        return;
    }

    // ===== 批办单模式：如果数据库已加载，从数据库读取记录 =====
    if (DbManager::instance()->isDatabaseLoaded()) {
        QList<ApprovalRecord> records = DbManager::instance()->getAllRecords();
        nlohmann::json root;
        root["success"] = true;
        root["mode"] = "approval";  // 批办单模式标识（前端据此调整展示）
        root["files"] = nlohmann::json::array();
        for (const auto& r : records) {
            nlohmann::json item;
            // 主键：直接使用数据库ID（批办单模式，前端id不再是 clientId|path）
            item["id"] = r.id.toStdString();
            item["mode"] = "approval";
            // 左侧列表只显示收文编号 A-26-1 作为名称
            item["filename"] = r.receiveNumber.toStdString();
            item["receiveNumber"] = r.receiveNumber.toStdString();
            item["wordCode"] = r.wordCode.toStdString();
            item["summary"] = r.summary.toStdString();
            item["department"] = r.department.toStdString();
            item["fileCategory"] = r.fileCategory.toStdString();
            item["receiveChannel"] = r.receiveChannel.toStdString();
            item["fileReceiveDate"] = r.fileReceiveDate.toStdString();
            item["emergencyLevel"] = r.emergencyLevel.toStdString();
            item["secretLevel"] = r.secretLevel.toStdString();
            item["leaderInstruction"] = r.leaderInstruction.toStdString();
            item["suggestion"] = r.suggestion.toStdString();
            item["processResult"] = r.processResult.toStdString();
            // 兼容旧字段：size / humanSize / previewSupported / ownerClient / relativePath
            item["ownerClient"] = "server";      // 表示服务端直接提供（非客户端文件）
            item["relativePath"] = r.receiveNumber.toStdString();
            item["createTime"] = r.createTime.toStdString();
            item["modifyTime"] = r.modifyTime.toStdString();
            item["size"] = 0;
            item["humanSize"] = "";
            item["previewSupported"] = true;
            root["files"].push_back(item);
        }
        HttpResponse response;
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse(root);
        sendResponse(socket, response);
        return;
    }

    // ===== 原有模式：从 FileIndexer 获取客户端文件 =====
    std::vector<FileMetadata> files = indexer_->getAllFiles();

    nlohmann::json root;
    root["success"] = true;
    root["files"] = nlohmann::json::array();
    for (const auto& file : files) {
        nlohmann::json item = Protocol::fileMetadataToJson(file);
        item["id"] = fileId(file).toStdString();
        item["humanSize"] = humanSize(file.size).toStdString();
        // 直接从文件名判断预览支持，避免每次都解析完整路径
        item["previewSupported"] = DocumentConverter::isPreviewSupported(QString::fromStdString(file.relativePath));
        root["files"].push_back(item);
    }

    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = jsonResponse(root);
    sendResponse(socket, response);
}

void WebServer::handleFileSearch(QTcpSocket* socket, const HttpRequest& request) {
    QString keyword = request.params.value("q").trimmed();

    // ===== 批办单模式：在数据库里搜索 =====
    if (DbManager::instance()->isDatabaseLoaded()) {
        QString username;
        if (!isAuthenticated(request, username)) {
            HttpResponse response;
            response.statusCode = 401;
            response.statusText = "Unauthorized";
            response.body = jsonResponse({{"success", false}, {"error", "未登录"}});
            sendResponse(socket, response);
            return;
        }
        QList<ApprovalRecord> records;
        if (keyword.isEmpty()) {
            records = DbManager::instance()->getAllRecords();
        } else {
            records = DbManager::instance()->searchRecords(keyword);
        }
        nlohmann::json root;
        root["success"] = true;
        root["mode"] = "approval";
        root["files"] = nlohmann::json::array();
        for (const auto& r : records) {
            nlohmann::json item;
            item["id"] = r.id.toStdString();
            item["mode"] = "approval";
            item["filename"] = r.receiveNumber.toStdString();
            item["receiveNumber"] = r.receiveNumber.toStdString();
            item["wordCode"] = r.wordCode.toStdString();
            item["summary"] = r.summary.toStdString();
            item["department"] = r.department.toStdString();
            item["fileCategory"] = r.fileCategory.toStdString();
            item["receiveChannel"] = r.receiveChannel.toStdString();
            item["fileReceiveDate"] = r.fileReceiveDate.toStdString();
            item["secretLevel"] = r.secretLevel.toStdString();
            item["ownerClient"] = "server";
            item["relativePath"] = r.receiveNumber.toStdString();
            item["createTime"] = r.createTime.toStdString();
            item["modifyTime"] = r.modifyTime.toStdString();
            item["size"] = 0;
            item["humanSize"] = "";
            item["previewSupported"] = true;
            root["files"].push_back(item);
        }
        HttpResponse response;
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse(root);
        sendResponse(socket, response);
        return;
    }

    // ===== 原有模式 =====
    std::vector<FileMetadata> files = indexer_->getAllFiles();

    nlohmann::json root;
    root["success"] = true;
    root["files"] = nlohmann::json::array();

    for (const auto& file : files) {
        QString filename = QString::fromStdString(file.filename);
        QString relativePath = QString::fromStdString(file.relativePath);
        if (!keyword.isEmpty() && !filename.contains(keyword, Qt::CaseInsensitive) &&
            !relativePath.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }

        nlohmann::json item = Protocol::fileMetadataToJson(file);
        item["id"] = fileId(file).toStdString();
        item["humanSize"] = humanSize(file.size).toStdString();
        root["files"].push_back(item);
    }

    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = jsonResponse(root);
    sendResponse(socket, response);
}

void WebServer::handleFileDownload(QTcpSocket* socket, const HttpRequest& request) {
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        sendResponse(socket, response);
        return;
    }

    if (!hasPermission(username, UserPermissionFlag::DownloadFile)) {
        HttpResponse response;
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "无权限下载文件"}});
        sendResponse(socket, response);
        return;
    }

    // ===== 批办单模式：id 是数据库主键，生成 Word+PDF 后返回 =====
    QString idParam = request.params.value("id").trimmed();
    QString formatParam = request.params.value("format").trimmed().toLower();
    if (DbManager::instance()->isDatabaseLoaded() && !idParam.contains('|')) {
        ApprovalRecord record;
        if (!DbManager::instance()->getRecordById(idParam, record)) {
            HttpResponse response;
            response.statusCode = 404;
            response.statusText = "Not Found";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = jsonResponse({{"success", false}, {"error", "找不到该批办单记录"}});
            sendResponse(socket, response);
            return;
        }
        QString templatePath;
        QString errMsg;
        QString outDocx, outPdf;
        QString generated = DbManager::instance()->generateDocument(record, templatePath, outDocx, outPdf, errMsg);
        if (generated.isEmpty()) {
            HttpResponse response;
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = jsonResponse({{"success", false}, {"error", "生成文档失败: " + errMsg.toStdString()}});
            sendResponse(socket, response);
            return;
        }
        AuditLogger::instance()->log(username, AuditAction::DownloadFile,
                                     record.receiveNumber, socket->peerAddress().toString(), true);
        // 默认返回 DOCX，format=pdf 时返回 PDF
        QString servePath = (formatParam == "pdf") ? outPdf : outDocx;
        QString serveName = (formatParam == "pdf")
                            ? record.receiveNumber + ".pdf"
                            : record.receiveNumber + ".docx";
        QFile f(servePath);
        if (!f.open(QIODevice::ReadOnly)) {
            HttpResponse response;
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.body = "无法读取生成的文档文件";
            sendResponse(socket, response);
            return;
        }
        HttpResponse response;
        response.headers["Content-Type"] = (formatParam == "pdf") ? "application/pdf" : "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        response.headers["Content-Disposition"] = "attachment; filename*=UTF-8''" + QString::fromLatin1(QUrl::toPercentEncoding(serveName));
        response.body = f.readAll();
        f.close();
        sendResponse(socket, response);
        return;
    }

    QString clientId;
    QString relativePath;
    if (!parseFileId(request.params.value("id"), clientId, relativePath)) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "Invalid file id";
        sendResponse(socket, response);
        return;
    }

    QString fullPath = indexer_->resolveFilePath(clientId, relativePath);
    QFileInfo info(fullPath);

    // 只从在线客户端获取文件，不使用服务器本地文件
    ClientHandler* handler = server_ ? server_->findClientHandler(clientId) : nullptr;

    if (!handler) {
        // 客户端未连接
        HttpResponse response;
        response.statusCode = 503;
        response.statusText = "Service Unavailable";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<html><body><h1>Client Not Connected</h1>"
                        "<p>The client that owns this file is not currently connected.</p></body></html>";
        sendResponse(socket, response);
        return;
    }

    // 从远程客户端获取 - 使用异步请求
    QPointer<QTcpSocket> socketPtr(socket);  // 使用 QPointer 跟踪 socket 生命周期
    QString usernameCapture = username;
    QString relativePathCapture = relativePath;

    handler->requestFileAsync(relativePath, [this, socketPtr, usernameCapture, relativePathCapture](const ClientHandler::FileRequestResult& result) {
        // 检查 socket 是否仍然有效
        if (!socketPtr) {
            return;  // Socket 已断开，不发送响应
        }

        if (result.success) {
            // 记录下载日志
            AuditLogger::instance()->log(usernameCapture, AuditAction::DownloadFile, relativePathCapture, socketPtr->peerAddress().toString(), true);

            HttpResponse response;
            response.headers["Content-Type"] = "application/octet-stream";
            response.headers["Content-Disposition"] = "attachment; filename*=UTF-8''" + QString::fromLatin1(QUrl::toPercentEncoding(QFileInfo(relativePathCapture).fileName()));
            response.body = result.data;
            sendResponse(socketPtr, response);
        } else {
            // 获取失败
            HttpResponse response;
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "text/html; charset=utf-8";
            response.body = "<html><body><h1>Download Failed</h1><p>" + result.error.toHtmlEscaped().toUtf8() + "</p></body></html>";
            sendResponse(socketPtr, response);
        }
    }, 60000);
}

void WebServer::handleBatchDownload(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证和权限
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        sendResponse(socket, response);
        return;
    }

    if (!hasPermission(username, UserPermissionFlag::BatchDownload)) {
        HttpResponse response;
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "无权限批量下载"}});
        sendResponse(socket, response);
        return;
    }

    QString idsParam = request.params.value("ids");
    if (idsParam.isEmpty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "Missing ids parameter";
        sendResponse(socket, response);
        return;
    }

    QStringList ids = idsParam.split(',', Qt::SkipEmptyParts);
    if (ids.isEmpty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "No file IDs provided";
        sendResponse(socket, response);
        return;
    }

    // ===== 批办单模式：ids 是数据库主键列表，生成 Word+PDF 后打包 =====
    QString formatParam = request.params.value("format").trimmed().toLower();
    if (DbManager::instance()->isDatabaseLoaded() && !ids.first().contains('|')) {
        QList<FileInfo> filePaths;
        for (const QString& id : ids) {
            ApprovalRecord record;
            if (!DbManager::instance()->getRecordById(id, record)) continue;
            QString templatePath, errMsg, outDocx, outPdf;
            QString generated = DbManager::instance()->generateDocument(record, templatePath, outDocx, outPdf, errMsg);
            if (generated.isEmpty()) continue;
            AuditLogger::instance()->log(username, AuditAction::DownloadFile,
                                         record.receiveNumber, socket->peerAddress().toString(), true);
            QString servePath = (formatParam == "pdf") ? outPdf : outDocx;
            QString serveName = (formatParam == "pdf")
                                ? record.receiveNumber + ".pdf"
                                : record.receiveNumber + ".docx";
            filePaths.append({servePath, serveName});
        }
        if (filePaths.isEmpty()) {
            HttpResponse response;
            response.statusCode = 404;
            response.statusText = "Not Found";
            response.body = "No documents could be generated";
            sendResponse(socket, response);
            return;
        }
        processBatchDownloadFiles(socket, filePaths);
        return;
    }

    // 使用异步方式收集所有文件
    auto filePaths = std::make_shared<QList<FileInfo>>();
    auto pendingCount = std::make_shared<int>(0);
    QPointer<QTcpSocket> socketPtr(socket);

    // 解析所有文件ID并准备请求
    struct FileRequest {
        ClientHandler* handler;
        QString relativePath;
        QString fileName;
    };
    QList<FileRequest> requests;

    for (const QString& id : ids) {
        QString clientId;
        QString relativePath;
        if (!parseFileId(id, clientId, relativePath)) {
            continue;
        }

        QString fullPath = indexer_->resolveFilePath(clientId, relativePath);
        QFileInfo info(fullPath);

        ClientHandler* handler = server_ ? server_->findClientHandler(clientId) : nullptr;
        if (handler) {
            requests.append({handler, relativePath, info.fileName()});
        }
    }

    if (requests.isEmpty()) {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.body = "No files found";
        sendResponse(socket, response);
        return;
    }

    *pendingCount = requests.size();

    // 异步请求所有文件
    for (const auto& req : requests) {
        req.handler->requestFileAsync(req.relativePath, [this, socketPtr, filePaths, pendingCount, fileName = req.fileName](const ClientHandler::FileRequestResult& result) {
            if (result.success && !result.data.isEmpty()) {
                // 保存到临时文件
                QString uniqueId = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz") + QString::number(qrand());
                QString tempPath = QDir::temp().filePath(uniqueId + "_" + fileName);
                QFile tempFile(tempPath);
                if (tempFile.open(QIODevice::WriteOnly)) {
                    tempFile.write(result.data);
                    tempFile.close();
                    filePaths->append({tempPath, fileName});
                }
            }

            (*pendingCount)--;

            // 所有文件都收集完成
            if (*pendingCount == 0) {
                if (!socketPtr || filePaths->isEmpty()) {
                    // Socket已断开或没有文件
                    if (socketPtr) {
                        HttpResponse response;
                        response.statusCode = 404;
                        response.statusText = "Not Found";
                        response.body = "No files available";
                        sendResponse(socketPtr, response);
                    }
                    return;
                }

                // 继续处理打包和发送
                processBatchDownloadFiles(socketPtr, *filePaths);
            }
        }, 60000);
    }
}

void WebServer::handleFilePreview(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证和权限
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<div class=\"empty\">未登录</div>";
        sendResponse(socket, response);
        return;
    }

    if (!hasPermission(username, UserPermissionFlag::PreviewFile)) {
        HttpResponse response;
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<div class=\"empty\">无权限预览文件</div>";
        sendResponse(socket, response);
        return;
    }

    QString id = request.params.value("id").trimmed();

    // ===== 批办单模式：id 是纯数字的数据库主键（不含 '|'）=====
    if (DbManager::instance()->isDatabaseLoaded() && !id.contains('|')) {
        ApprovalRecord record;
        if (!DbManager::instance()->getRecordById(id, record)) {
            HttpResponse response;
            response.statusCode = 404;
            response.headers["Content-Type"] = "text/html; charset=utf-8";
            response.body = "<div class=\"empty\">找不到该批办单记录</div>";
            sendResponse(socket, response);
            return;
        }
        // 记录审计
        AuditLogger::instance()->log(username, AuditAction::PreviewFile,
                                     record.receiveNumber, socket->peerAddress().toString(), true);
        // HTML 模板预览：读取 HTML 模板，注入 fillForm() 调用，直接返回 HTML
        QString htmlTemplatePath = QCoreApplication::applicationDirPath() + "/文件批办单.html";
        QFile htmlFile(htmlTemplatePath);
        if (!htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            HttpResponse response;
            response.statusCode = 200;
            response.headers["Content-Type"] = "text/html; charset=utf-8";
            response.body = QString("<div style='padding:24px;color:#991b1b'>找不到HTML模板文件: %1</div>").arg(htmlTemplatePath).toUtf8();
            sendResponse(socket, response);
            return;
        }
        QString htmlContent = QString::fromUtf8(htmlFile.readAll());
        htmlFile.close();
        // 格式化日期: "2026-09-15" → "2026-9-15" (去掉月日前导零)
        QString formattedDate = record.fileReceiveDate;
        QRegularExpression dateRe("(\\d{4})-(\\d{1,2})-(\\d{1,2})");
        auto dateMatch = dateRe.match(formattedDate);
        if (dateMatch.hasMatch()) {
            formattedDate = QString("%1-%2-%3")
                .arg(dateMatch.captured(1).toInt())
                .arg(dateMatch.captured(2).toInt())
                .arg(dateMatch.captured(3).toInt());
        }
        // 构建 fillForm 数据 JSON
        auto esc = [](const QString& s) -> QString {
            QString r = s;
            r.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "").replace("\t", "\\t");
            return "\"" + r + "\"";
        };
        QString fillData = "{"
            "laiwenDanwei:" + esc(record.department) + ","
            "laiwenZihao:" + esc(record.wordCode) + ","
            "shouwenRiqi:" + esc(formattedDate) + ","
            "laiwenLeixing:" + esc(record.fileCategory) + ","
            "shouwenTujing:" + esc(record.receiveChannel) + ","
            "jinjiChengdu:" + esc(record.emergencyLevel) + ","
            "miJi:" + esc(record.secretLevel) + ","
            "shouwenBianhao:" + esc(record.receiveNumber) + ","
            "wenjianBiaoti:" + esc(record.summary) + ","
            "lingdaoPishi:" + esc(record.leaderInstruction) + ","
            "niBanYijian:" + esc(record.suggestion) + ","
            "chuanYueNames:[],"
            "chuanYueDates:[],"
            "banliJieguo:" + esc(record.processResult) +
            "}";
        // 在 </body> 前注入 fillForm 调用
        QString injectScript = QString(
            "<script>\n"
            "if(document.fonts && document.fonts.ready){\n"
            "  document.fonts.ready.then(function(){ fillForm(%1); });\n"
            "}else{\n"
            "  setTimeout(function(){ fillForm(%1); }, 200);\n"
            "}\n"
            "</script>\n"
            "</body>"
        ).arg(fillData);
        htmlContent.replace("</body>", injectScript, Qt::CaseInsensitive);
        HttpResponse response;
        response.statusCode = 200;
        response.statusText = "OK";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = htmlContent.toUtf8();
        sendResponse(socket, response);
        return;
    }

    // ===== 原有模式：从远程客户端获取预览 =====
    QString clientId;
    QString relativePath;
    if (!parseFileId(id, clientId, relativePath)) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "Invalid file id";
        sendResponse(socket, response);
        return;
    }

    QString fullPath = indexer_->resolveFilePath(clientId, relativePath);
    QFileInfo info(fullPath);

    // 只从在线客户端获取文件
    ClientHandler* handler = server_ ? server_->findClientHandler(clientId) : nullptr;

    if (!handler) {
        // 客户端未连接
        HttpResponse response;
        response.statusCode = 503;
        response.statusText = "Service Unavailable";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<div class=\"empty\">Client not connected. Cannot preview remote file.</div>";
        sendResponse(socket, response);
        return;
    }

    // 从远程客户端获取 - 使用异步请求
    QPointer<QTcpSocket> socketPtr(socket);
    QString relativePathCapture = relativePath;

    handler->requestFileAsync(relativePath, [this, socketPtr, relativePathCapture](const ClientHandler::FileRequestResult& result) {
        if (!socketPtr) return;
        if (result.success) {
            QString uniqueId = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz");
            QString suffix = QFileInfo(relativePathCapture).suffix();
            QString tempPath = QDir::temp().filePath(
                "preview_" + uniqueId + (suffix.isEmpty() ? QString() : "." + suffix));
            QFile tempFile(tempPath);
            if (tempFile.open(QIODevice::WriteOnly)) {
                tempFile.write(result.data);
                tempFile.close();

                auto preview = DocumentConverter::previewFile(tempPath);
                HttpResponse response;
                if (!preview.success) {
                    response.statusCode = 415;
                    response.statusText = "Unsupported Media Type";
                    response.headers["Content-Type"] = "text/html; charset=utf-8";
                    response.body = "<div class=\"empty\">" + preview.error.toHtmlEscaped().toUtf8() + "</div>";
                } else {
                    response.headers["Content-Type"] = preview.mimeType;
                    response.body = preview.data;
                }
                QFile::remove(tempPath);
                sendResponse(socketPtr, response);
                return;
            }
        }
        HttpResponse response;
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<div class=\"empty\">Preview failed: " + result.error.toHtmlEscaped().toUtf8() + "</div>";
        sendResponse(socketPtr, response);
    }, 60000);
}


void WebServer::serveStaticFile(QTcpSocket* socket, const QString& path) {
    Q_UNUSED(path)
    HttpResponse response;
    response.statusCode = 404;
    response.statusText = "Not Found";
    response.body = "Not Found";
    sendResponse(socket, response);
}

void WebServer::serveLoginPage(QTcpSocket* socket) {
    static const char* html = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>批办单查询系统 登录</title>
<style>body{font-family:Segoe UI,Microsoft YaHei,sans-serif;background:#f3f6fb;margin:0;display:grid;place-items:center;height:100vh}.card{width:360px;background:white;border-radius:14px;box-shadow:0 20px 50px #1d35571f;padding:32px}h1{margin:0 0 8px;color:#1d3557}.muted{color:#667085;margin-bottom:24px}.field{margin:14px 0}label{display:block;font-size:13px;color:#344054;margin-bottom:6px}input{width:100%;box-sizing:border-box;border:1px solid #d0d5dd;border-radius:8px;padding:11px;font-size:15px}button{width:100%;margin-top:18px;background:#2563eb;color:white;border:0;border-radius:8px;padding:12px;font-size:15px;cursor:pointer}.error{color:#b42318;margin-top:14px;min-height:20px}</style></head><body><div class="card"><h1>批办单查询系统</h1><div class="muted">请输入用户名和密码访问共享文件</div><div class="field"><label>用户名</label><input id="u" value="admin"></div><div class="field"><label>密码</label><input id="p" type="password" placeholder="默认：admin123"></div><button onclick="login()">登录</button><div id="e" class="error"></div></div><script>
async function login(){const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:u.value,password:p.value})});const j=await r.json();if(j.success) location.href=j.redirect; else e.textContent=j.error||'登录失败'}
document.addEventListener('keydown',ev=>{if(ev.key==='Enter')login()});
</script></body></html>)HTML";
    HttpResponse response;
    response.headers["Content-Type"] = "text/html; charset=utf-8";
    response.body = html;
    sendResponse(socket, response);
}

void WebServer::serveBrowsePage(QTcpSocket* socket) {
    static const char* htmlTemplate = R"HTML(
<!doctype html><html lang="zh-CN"><head> <meta charset="utf-8"> <meta name="viewport" content="width=device-width,initial-scale=1"> <title>批办单查询系统</title> <style> body{font-family:Segoe UI,Microsoft YaHei,sans-serif;margin:0;color:#101828;background:#f6f8fb} .top{height:64px;background:#1d3557;color:white;display:flex;align-items:center;gap:16px;padding:0 22px} .top h1{font-size:20px;margin:0} .search{flex:1;max-width:800px;display:flex;gap:8px;align-items:center} .search input{width:100%;border:0;border-radius:8px;padding:10px} .search button{border:0;border-radius:8px;padding:10px 16px;cursor:pointer;background:#10b981;color:white;font-size:14px;white-space:nowrap} .search button:disabled{opacity:0.5;cursor:not-allowed} .date-filter{display:flex;gap:8px;align-items:center;color:white;font-size:14px} .date-filter input[type="date"]{border:0;border-radius:6px;padding:6px;color:#111827} .top-actions{margin-left:auto;display:flex;gap:8px;align-items:center} .btn{border:0;border-radius:8px;padding:10px 14px;cursor:pointer;background:#2563eb;color:white;text-decoration:none;font-size:14px;white-space:nowrap} .btn.secondary{background:#e5e7eb;color:#111827} .btn:disabled{opacity:0.5;cursor:not-allowed} .layout{display:flex;height:calc(100vh - 64px)} .list{width:420px;min-width:200px;max-width:800px;border-right:1px solid #e4e7ec;background:white;overflow:auto} .list-header{padding:10px 14px;border-bottom:2px solid #e4e7ec;background:#f9fafb;display:flex;gap:8px;align-items:center} .client-filter{padding:10px 14px;border-bottom:1px solid #e4e7ec;background:white} .client-filter select{width:100%;border:1px solid #d1d5db;border-radius:6px;padding:8px;font-size:14px;color:#111827;background:white} .item{padding:12px 14px;border-bottom:1px solid #eef2f7;cursor:pointer;display:flex;gap:10px;align-items:start} .item:hover{background:#eff6ff} .item.active{background:#dbeafe} .item input[type="checkbox"]{margin-top:4px;width:16px;height:16px;cursor:pointer} .item-content{flex:1;min-width:0} .name{font-weight:600} .meta{font-size:12px;color:#667085;margin-top:4px} .badge{display:inline-block;background:#10b981;color:white;padding:2px 6px;border-radius:4px;font-size:11px;margin-left:6px} .preview{flex:1;display:flex;flex-direction:column;min-width:0;max-height:calc(100vh - 64px);overflow:hidden} .toolbar{background:white;border-bottom:1px solid #e4e7ec;padding:10px;display:flex;gap:8px;align-items:center;overflow-x:auto;flex-shrink:0} .frame{border:0;background:white;margin:16px;border-radius:10px;box-shadow:0 1px 3px #0000001a;width:100%;height:100%;flex:1;min-height:0} .empty{padding:40px;color:#667085;text-align:center} .text-preview{white-space:pre-wrap;padding:20px} .count{color:#667085;font-size:13px} .resizer{width:5px;cursor:col-resize;background:#f3f4f6;flex-shrink:0} .resizer:hover{background:#d1d5db} </style>)HTML" R"HTML(</head><body> <div class="top"> <h1>批办单查询系统 {{VERSION}}</h1> <div class="search"> <input id="q" placeholder="文件名过滤 / 按回车全文搜索"> <button id="contentSearchBtn" onclick="doContentSearch()">全文搜索</button> </div> <div class="date-filter"> <label>日期筛选:</label> <input type="date" id="dateFrom" style="width:150px"> <button id="selectByDateBtn" class="btn" onclick="selectByDate()">选择</button> <button id="clearDateBtn" class="btn secondary" onclick="clearDateFilter()">清除</button> </div> <div class="top-actions"> <button class="btn" onclick="downloadSelected()" id="batchDownloadBtn" disabled>批量下载</button> <a class="btn secondary" href="/api/logout">退出</a> </div> </div> <div class="layout"> <div class="list" id="fileList"> <div class="client-filter"> <select id="clientFilter" onchange="onDbFilterChange()"> <option value="">请选择数据库</option> </select> </div> <div class="list-header"> <input type="checkbox" id="selectAll" onchange="toggleSelectAll()"> <label for="selectAll" style="flex:1;cursor:pointer">全选 / 全不选</label> <span id="count" class="count"></span> </div> <div id="list"> <div class="empty">正在加载...</div> </div> </div> <div class="resizer" id="resizer"></div> <div class="preview"> <div class="toolbar"> <strong id="title" style="flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">请选择文件</strong> <button id="watermarkBtn" class="btn secondary" style="display:none" onclick="generateWatermark()">添加水印导出</button> <button id="download" class="btn" style="display:none" onclick="downloadFile()">下载</button> <button id="printBtn" class="btn secondary" style="display:none" onclick="printPreview()">打印</button> <button class="btn secondary" onclick="zoomPreview(0.1)" title="放大" style="padding:10px 12px;font-size:16px">＋</button> <button class="btn secondary" onclick="zoomPreview(-0.1)" title="缩小" style="padding:10px 12px;font-size:16px">－</button> <button class="btn secondary" onclick="zoomReset()" title="复原" style="padding:10px 12px">1:1</button> <span id="zoomLabel" style="font-size:12px;color:#667085;min-width:40px">100%</span> </div> <iframe id="frame" class="frame" srcdoc="<div class='empty'>选择左侧文件后在这里预览</div>"></iframe> </div> </div> <script> let files = [], selected = null, userPermissions = {}, watermarkEnabled = false, selectedIds = new Set(), contentSearchResults = [], isContentSearchMode = false, isApprovalMode = false; function fmtDate(s) { if (!s) return ''; const m = String(s).match(/^(\d{4})-(\d{1,2})-(\d{1,2})/); return m ? (parseInt(m[1]) + '-' + parseInt(m[2]) + '-' + parseInt(m[3])) : s; } async function loadDatabases() { try { const r = await fetch('/api/databases'); if (!r.ok) return; const j = await r.json(); if (!j.success) return; const select = document.getElementById('clientFilter'); select.innerHTML = ''; for (const db of j.databases) { const opt = document.createElement('option'); opt.value = db.path; opt.textContent = db.name; if (db.active) opt.selected = true; select.appendChild(opt); } } catch(e) { console.error('loadDatabases error:', e); } } async function onDbFilterChange() { const select = document.getElementById('clientFilter'); const path = select.value; if (!path) return; try { await fetch('/api/databases/select?path=' + encodeURIComponent(path)); await loadFiles(); render(); } catch(e) { console.error('db select error:', e); } } async function loadFiles() { const r = await fetch('/api/files'); if (r.status === 401) { location.href = '/login'; return; } const j = await r.json(); files = j.files || []; isApprovalMode = (j.mode === 'approval'); files.sort((a, b) => new Date(b.createTime).getTime() - new Date(a.createTime).getTime()); updateClientFilter(); render(); } function updateClientFilter() {
if (isApprovalMode) { const cf = document.getElementById('clientFilter'); if (cf) { cf.parentElement.style.display = ''; loadDatabases(); } const csb = document.getElementById('contentSearchBtn'); if (csb) csb.style.display = 'none'; return; }
const clients = [...new Set(files.map(f => f.ownerClient))].sort(); const select = document.getElementById('clientFilter'); const currentValue = select.value; select.innerHTML = '<option value="">全部客户端</option>'; for (const client of clients) { const option = document.createElement('option'); option.value = client; option.textContent = client; select.appendChild(option); } select.value = currentValue; } async function doContentSearch() { const query = document.getElementById('q').value.trim(); if (!query) { alert('请输入搜索关键词'); return; } const btn = document.getElementById('contentSearchBtn'); btn.disabled = true; btn.textContent = '搜索中...'; try { const r = await fetch('/api/content-search', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ query: query }) }); if (r.status === 401) { location.href = '/login'; return; } const j = await r.json(); if (j.success) { contentSearchResults = j.results || []; isContentSearchMode = true; render(); } else { alert('搜索失败: ' + (j.error || '未知错误')); } } catch (e) { alert('搜索失败: ' + e.message); } finally { btn.disabled = false; btn.textContent = '全文搜索'; } } function render() { const query = document.getElementById('q').value.trim().toLowerCase(); const selectedClient = document.getElementById('clientFilter').value; let displayFiles = []; let countText = ''; if (isContentSearchMode && contentSearchResults.length > 0) { displayFiles = contentSearchResults.map(r => ({ id: r.ownerClient + '|' + r.relativePath, filename: r.filename, relativePath: r.relativePath, ownerClient: r.ownerClient, size: r.size, humanSize: '', modifyTime: r.modifyTime, createTime: '', isContentMatch: true })); const filtered = displayFiles.filter(f => { const matchClient = isApprovalMode || (!selectedClient || f.ownerClient === selectedClient); return matchClient; }); displayFiles = filtered; countText = filtered.length + ' 个全文搜索结果'; } else { isContentSearchMode = false; contentSearchResults = []; const filtered = files.filter(f => { const matchQuery = !query || [f.filename,f.relativePath,f.summary,f.suggestion,f.department,f.wordCode,f.leaderInstruction,f.processResult,f.receiveChannel,f.fileCategory,f.emergencyLevel,f.secretLevel].some(v=>v&&v.toLowerCase().includes(query)); const matchClient = isApprovalMode || (!selectedClient || f.ownerClient === selectedClient); return matchQuery && matchClient; }); displayFiles = filtered; countText = filtered.length + ' / ' + files.length + ' 个文件'; } document.getElementById('count').textContent = countText; const listEl = document.getElementById('list'); listEl.innerHTML = ''; if (!displayFiles.length) { listEl.innerHTML = '<div class="empty">没有匹配的文件</div>'; return; } for (const f of displayFiles) { const div = document.createElement('div'); div.className = 'item'; const cb = document.createElement('input'); cb.type = 'checkbox'; cb.checked = selectedIds.has(f.id); cb.onchange = (e) => { e.stopPropagation(); toggleSelect(f.id); }; div.appendChild(cb); const content = document.createElement('div'); content.className = 'item-content'; content.onclick = () => selectFile(f, div); const badge = f.isContentMatch ? '<span class="badge">全文</span>' : ''; content.innerHTML = isApprovalMode ?
            '<div class="name">' + escapeHtml(f.filename) + badge + '</div>' +
            '<div class="meta">' + escapeHtml(f.wordCode || '') + '</div>' +
            '<div class="meta">' + escapeHtml(fmtDate(f.fileReceiveDate)) + '</div>' +
            '<div class="meta">' + escapeHtml(f.summary || '') + '</div>'
          : '<div class="name">' + escapeHtml(f.filename) + badge + '</div>' + '<div class="meta">' + escapeHtml(f.ownerClient) + ' · ' + escapeHtml(f.relativePath) + ' · ' + escapeHtml(f.humanSize || '') + '</div>' + '<div class="meta">创建时间：' + escapeHtml(f.createTime || f.modifyTime || '') + '</div>'; div.appendChild(content); listEl.appendChild(div); } updateBatchButton(); } function toggleSelect(id) { if (selectedIds.has(id)) selectedIds.delete(id); else selectedIds.add(id); updateBatchButton(); document.getElementById('selectAll').checked = selectedIds.size === files.length; } function toggleSelectAll() { const checked = document.getElementById('selectAll').checked; if (checked) { files.forEach(f => selectedIds.add(f.id)); } else { selectedIds.clear(); } render(); } function updateBatchButton() { const btn = document.getElementById('batchDownloadBtn'); btn.disabled = selectedIds.size === 0; btn.textContent = '批量下载' + (selectedIds.size > 0 ? ' (' + selectedIds.size + ')' : ''); } function selectByDate() { const dateFrom = document.getElementById('dateFrom').value; if (!dateFrom) { alert('请选择起始日期'); return; } const fromTime = new Date(dateFrom).getTime(); selectedIds.clear(); files.forEach(f => { const ds = isApprovalMode ? (f.fileReceiveDate || f.createTime) : f.createTime; const ct = new Date(ds).getTime(); if (ct >= fromTime) selectedIds.add(f.id); }); render(); } function clearDateFilter() { document.getElementById('dateFrom').value = ''; selectedIds.clear(); render(); } function selectFile(f, el) { selected = f; document.querySelectorAll('.item').forEach(x => x.classList.remove('active')); el.classList.add('active'); document.getElementById('title').textContent = f.filename; const downloadBtn = document.getElementById('download'); if (downloadBtn && userPermissions.downloadFile) { downloadBtn.style.display = 'inline-block'; } const printBtn = document.getElementById('printBtn'); if (printBtn && userPermissions.printFile) { printBtn.style.display = 'inline-block'; } const watermarkBtn = document.getElementById('watermarkBtn'); if (watermarkBtn) {
const isWordDoc = f.filename.toLowerCase().endsWith('.doc') || f.filename.toLowerCase().endsWith('.docx');
const isApprovalDoc = isApprovalMode;
if ((isWordDoc || isApprovalDoc) && userPermissions.watermarkExport && (watermarkEnabled || isApprovalMode)) {
watermarkBtn.style.display = 'inline-block';
}
else {
watermarkBtn.style.display = 'none';
}
} if (userPermissions.previewFile) { const frameEl = document.getElementById('frame'); frameEl.removeAttribute('srcdoc'); frameEl.src = '/api/preview?id=' + encodeURIComponent(f.id); } } function downloadFile() { if (selected) location.href = '/api/download?id=' + encodeURIComponent(selected.id); } async function downloadSelected() { if (selectedIds.size === 0) return; const ids = Array.from(selectedIds); if (ids.length === 1) { location.href = '/api/download?id=' + encodeURIComponent(ids[0]); return; } location.href = '/api/batch-download?ids=' + encodeURIComponent(ids.join(',')); } function printPreview() { const frameEl = document.getElementById('frame'); frameEl.contentWindow.focus(); frameEl.contentWindow.print(); } var zoomLevel = 1; function zoomPreview(delta) { zoomLevel = Math.max(0.5, Math.min(3, zoomLevel + delta)); applyZoom(); } function zoomReset() { zoomLevel = 1; applyZoom(); } function applyZoom() { document.getElementById('frame').style.zoom = zoomLevel; document.getElementById('zoomLabel').textContent = Math.round(zoomLevel * 100) + '%'; } async function generateWatermark() { if (!selected) return; const btn = document.getElementById('watermarkBtn'); btn.disabled = true; btn.textContent = '生成中...'; try { const res = await fetch('/api/watermark/generate', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ filePath: selected.relativePath, clientId: selected.ownerClient, isApproval: isApprovalMode, id: selected.id }) }); if (!res.ok) { const err = await res.json(); alert('生成水印失败: ' + (err.error || '未知错误')); return; } const contentType = res.headers.get('Content-Type'); if (contentType && contentType.includes('application/json')) { const data = await res.json(); console.log('[Watermark] Response data:', data); if (data.mode === 'individual') { console.log('[Watermark] Individual mode, sessionId:', data.sessionId, 'files:', data.files); btn.textContent = '下载中 (0/' + data.files.length + ')...'; for (let i = 0; i < data.files.length; i++) { const fileName = data.files[i]; const downloadUrl = '/api/watermark/download?sessionId=' + encodeURIComponent(data.sessionId) + '&fileName=' + encodeURIComponent(fileName); console.log('[Watermark] Downloading:', downloadUrl); const downloadRes = await fetch(downloadUrl); if (!downloadRes.ok) { alert('下载 ' + fileName + ' 失败'); continue; } const blob = await downloadRes.blob(); const url = window.URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = fileName; document.body.appendChild(a); a.click(); window.URL.revokeObjectURL(url); document.body.removeChild(a); btn.textContent = '下载中 (' + (i+1) + '/' + data.files.length + ')...'; await new Promise(r => setTimeout(r, 200)); } } else { alert('未知响应格式'); } } else { const blob = await res.blob(); const url = window.URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = selected.filename.replace(/\.[^.]+$/, '_水印图片.zip'); document.body.appendChild(a); a.click(); window.URL.revokeObjectURL(url); document.body.removeChild(a); } } catch (e) { alert('生成水印失败: ' + e.message); } finally { btn.disabled = false; btn.textContent = '添加水印导出'; } } function escapeHtml(s) { return String(s || '').replace(/[&<>"']/g, m => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[m])); } document.getElementById('q').addEventListener('input', () => { if (isContentSearchMode && document.getElementById('q').value.trim() === '') { isContentSearchMode = false; contentSearchResults = []; } render(); }); document.getElementById('q').addEventListener('keydown', (e) => { if (e.key === 'Enter') { doContentSearch(); } }); const resizer = document.getElementById('resizer'); const leftPanel = document.getElementById('fileList'); let isResizing = false; resizer.addEventListener('mousedown', (e) => { isResizing = true; document.body.style.cursor = 'col-resize'; }); document.addEventListener('mousemove', (e) => { if (!isResizing) return; const newWidth = e.clientX; if (newWidth >= 200 && newWidth <= 800) { leftPanel.style.width = newWidth + 'px'; } }); document.addEventListener('mouseup', () => { isResizing = false; document.body.style.cursor = ''; }); async function loadUserPermission() { try { const res = await fetch("/api/user"); const data = await res.json(); if (data.success) { userPermissions = data.user.permissionFlags || {}; watermarkEnabled = data.watermarkEnabled || false; applyPermissionRestrictions(); } } catch (e) { console.error("Failed to load user permission", e); } } function applyPermissionRestrictions() { if (!userPermissions.viewFileList) { document.querySelector(".list")?.remove(); } if (!userPermissions.printFile) { document.getElementById("printBtn")?.remove(); } if (!userPermissions.downloadFile) { document.getElementById("download")?.remove(); } if (!userPermissions.batchDownload) { document.getElementById("batchDownloadBtn")?.remove(); } if (!userPermissions.watermarkExport) { document.getElementById("watermarkBtn")?.remove(); } if (!userPermissions.dateFilter) { document.querySelector(".date-filter")?.remove(); } } loadFiles(); loadUserPermission(); setInterval(() => { if (document.visibilityState === 'visible') { loadFiles(); } }, 3000); </script></body></html>)HTML";

    // 替换版本号占位符
    QString html = QString::fromUtf8(htmlTemplate);
    html.replace("{{VERSION}}", PROJECT_VERSION);

    HttpResponse response;
    response.headers["Content-Type"] = "text/html; charset=utf-8";
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate";
    response.headers["Pragma"] = "no-cache";
    response.headers["Expires"] = "0";
    response.body = html.toUtf8();
    sendResponse(socket, response);
}

QString WebServer::getMimeType(const QString& filename) {
    QString suffix = QFileInfo(filename).suffix().toLower();
    if (suffix == "html") return "text/html; charset=utf-8";
    if (suffix == "css") return "text/css; charset=utf-8";
    if (suffix == "js") return "application/javascript; charset=utf-8";
    if (suffix == "json") return "application/json; charset=utf-8";
    if (suffix == "png") return "image/png";
    if (suffix == "jpg" || suffix == "jpeg") return "image/jpeg";
    if (suffix == "gif") return "image/gif";
    if (suffix == "pdf") return "application/pdf";
    return "application/octet-stream";
}

bool WebServer::isAuthenticated(const HttpRequest& request, QString& username) {
    if (!authManager_) {
        return false;
    }
    return authManager_->validateSession(cookieValue(request, "CNS_SESSION"), username);
}

bool WebServer::hasPermission(const QString& username, UserPermissionFlag permission) {
    return UserManager::instance()->hasPermission(username, permission);
}

QByteArray WebServer::buildHttpResponse(const HttpResponse& response) {
    QByteArray data;
    data += "HTTP/1.1 " + QByteArray::number(response.statusCode) + " " + response.statusText.toUtf8() + "\r\n";
    QMap<QString, QString> headers = response.headers;
    headers["Content-Length"] = QString::number(response.body.size());
    headers["Connection"] = "close";
    headers["X-Content-Type-Options"] = "nosniff";
    if (!headers.contains("Content-Type")) {
        headers["Content-Type"] = "text/plain; charset=utf-8";
    }
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        data += it.key().toUtf8() + ": " + it.value().toUtf8() + "\r\n";
    }
    data += "\r\n";
    data += response.body;
    return data;
}

void WebServer::handleWatermarkGenerate(QTcpSocket* socket, const HttpRequest& request) {
    HttpResponse response;

    // 检查认证
    QString username;
    if (!isAuthenticated(request, username)) {
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"未登录"})";
        sendResponse(socket, response);
        return;
    }

    // 检查水印导出权限
    if (!hasPermission(username, UserPermissionFlag::WatermarkExport)) {
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"无权限导出水印文档"})";
        sendResponse(socket, response);
        return;
    }

    // 检查水印服务是否可用
    if (!watermarkService_) {
        qDebug() << "[Watermark] Error: Watermark service not initialized";
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"水印服务未初始化"})";
        sendResponse(socket, response);
        return;
    }

    qDebug() << "[Watermark] Starting watermark generation request";

    // 解析请求参数
    QJsonDocument doc = QJsonDocument::fromJson(request.body);
    if (!doc.isObject()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"无效的请求参数"})";
        sendResponse(socket, response);
        return;
    }

    QJsonObject obj = doc.object();
    QString filePath = obj["filePath"].toString();
    QString clientId = obj["clientId"].toString();
    bool isApproval = obj["isApproval"].toBool();
    QString approvalId = obj["id"].toString();

    // 检查水印功能是否启用（批办单模式跳过此检查）
    qDebug() << "[Watermark] Watermark service config enabled:" << watermarkService_->getConfig().enabled;
    if (!isApproval && !watermarkService_->getConfig().enabled) {
        qDebug() << "[Watermark] Error: Watermark feature is disabled in config";
        response.statusCode = 403;
        response.statusText = "Forbidden";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"水印功能未启用"})";
        sendResponse(socket, response);
        return;
    }

    // ===== 批办单模式：从数据库生成 PDF，再加水印 =====
    if (isApproval && !approvalId.isEmpty() && DbManager::instance()->isDatabaseLoaded()) {
        ApprovalRecord record;
        if (!DbManager::instance()->getRecordById(approvalId, record)) {
            response.statusCode = 404;
            response.statusText = "Not Found";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = R"({"success":false,"error":"找不到该批办单记录"})";
            sendResponse(socket, response);
            return;
        }
        QString templatePath, errMsg, outDocx, outPdf;
        QString generated = DbManager::instance()->generateDocument(record, templatePath, outDocx, outPdf, errMsg);
        if (generated.isEmpty()) {
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = jsonResponse({{"success", false}, {"error", "生成文档失败: " + errMsg.toStdString()}});
            sendResponse(socket, response);
            return;
        }
        qDebug() << "[Watermark] generateDocument returned:" << generated << "outDocx:" << outDocx;
        AuditLogger::instance()->log(username, AuditAction::DownloadFile,
                                     record.receiveNumber, socket->peerAddress().toString(), true);
        // 创建临时目录用于水印处理
        QString tempDir = QDir::tempPath() + "/crossnet_wm_approval_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempDir);
        QString tempDocxPath = tempDir + "/" + record.receiveNumber + ".docx";
        if (!QFile::copy(outDocx, tempDocxPath)) {
            qDebug() << "[Watermark] Failed to copy" << outDocx << "to" << tempDocxPath;
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = jsonResponse({{"success", false}, {"error", "无法复制生成的文档"}});
            sendResponse(socket, response);
            return;
        }
        qDebug() << "[Watermark] Copied to tempDocxPath:" << tempDocxPath;
        processWatermarkGeneration(socket, tempDir, tempDocxPath, username, record.suggestion);
        return;
    }

    if (filePath.isEmpty() || clientId.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"缺少必要参数"})";
        sendResponse(socket, response);
        return;
    }

    // 查找客户端处理器
    if (!server_) {
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"服务器未初始化"})";
        sendResponse(socket, response);
        return;
    }

    ClientHandler* handler = server_->findClientHandler(clientId);
    if (!handler) {
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"客户端未连接"})";
        sendResponse(socket, response);
        return;
    }

    // 异步请求客户端发送文件数据
    QPointer<QTcpSocket> socketPtr(socket);

    handler->requestFileAsync(filePath, [this, socketPtr, filePath, username](const ClientHandler::FileRequestResult& result) {
        if (!socketPtr) {
            return; // Socket已断开
        }

        HttpResponse response;

        if (!result.success) {
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            QJsonObject errorObj;
            errorObj["success"] = false;
            errorObj["error"] = result.error;
            response.body = QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
            sendResponse(socketPtr, response);
            return;
        }

        // 创建临时目录
        QString tempDir = QDir::tempPath() + "/crossnet_watermark_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempDir);

        // 保存原始文件到临时目录
        QString originalFileName = QFileInfo(filePath).fileName();
        QString tempFilePath = tempDir + "/" + originalFileName;

        QFile tempFile(tempFilePath);
        if (!tempFile.open(QIODevice::WriteOnly)) {
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = R"({"success":false,"error":"无法创建临时文件"})";
            sendResponse(socketPtr, response);
            QDir(tempDir).removeRecursively();
            return;
        }

        tempFile.write(result.data);
        tempFile.close();

        // 继续水印处理
        processWatermarkGeneration(socketPtr, tempDir, tempFilePath, username);
    }, 30000);
}

void WebServer::handleWatermarkDownload(QTcpSocket* socket, const HttpRequest& request) {
    HttpResponse response;

    qDebug() << "[Watermark Download] Request path:" << request.path;

    // 检查认证
    QString username;
    if (!isAuthenticated(request, username)) {
        qDebug() << "[Watermark Download] Authentication failed";
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"未登录"})";
        sendResponse(socket, response);
        return;
    }

    // 解析查询参数
    QString sessionId = request.params.value("sessionId");
    QString fileName = request.params.value("fileName");

    qDebug() << "[Watermark Download] sessionId:" << sessionId << "fileName:" << fileName;

    if (sessionId.isEmpty() || fileName.isEmpty()) {
        qDebug() << "[Watermark Download] Missing parameters";
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"缺少sessionId或fileName参数"})";
        sendResponse(socket, response);
        return;
    }

    // 查找会话
    if (!watermarkSessions_.contains(sessionId)) {
        qDebug() << "[Watermark Download] Session not found:" << sessionId;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"会话不存在或已过期"})";
        sendResponse(socket, response);
        return;
    }

    WatermarkSession session = watermarkSessions_[sessionId];
    QString filePath = session.tempDir + "/" + fileName;

    qDebug() << "[Watermark Download] File path:" << filePath;

    // 读取文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[Watermark Download] Failed to open file:" << filePath;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"success":false,"error":"文件不存在"})";
        sendResponse(socket, response);
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    qDebug() << "[Watermark Download] Sending file, size:" << fileData.size() << "bytes";

    // 返回图片文件
    response.statusCode = 200;
    response.statusText = "OK";
    response.headers["Content-Type"] = "image/jpeg";
    response.headers["Content-Disposition"] = "attachment; filename=\"" + fileName.toUtf8() + "\"";
    response.body = fileData;

    sendResponse(socket, response);

    // 检查是否所有文件都已下载，如果是则清理会话
    session.fileNames.removeOne(fileName);
    if (session.fileNames.isEmpty()) {
        QDir(session.tempDir).removeRecursively();
        watermarkSessions_.remove(sessionId);
        qDebug() << "[Watermark Download] Session" << sessionId << "completed and cleaned up";
    } else {
        watermarkSessions_[sessionId] = session;
        qDebug() << "[Watermark Download] Remaining files:" << session.fileNames.size();
    }
}

void WebServer::processBatchDownloadFiles(QTcpSocket* socket, const QList<FileInfo>& files) {
    if (files.isEmpty()) {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.body = "No files available";
        sendResponse(socket, response);
        return;
    }

    // 如果只有一个文件，直接下载
    if (files.size() == 1) {
        QFile file(files[0].tempPath);
        if (file.open(QIODevice::ReadOnly)) {
            HttpResponse response;
            response.headers["Content-Type"] = "application/octet-stream";
            response.headers["Content-Disposition"] = "attachment; filename*=UTF-8''" + QString::fromLatin1(QUrl::toPercentEncoding(files[0].fileName));
            response.body = file.readAll();
            file.close();
            sendResponse(socket, response);

            // 清理临时文件
            QFile::remove(files[0].tempPath);
            return;
        }
    }

    // 多个文件需要打包成 ZIP
    QString uniqueId = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz");
    QString tempDir = QDir::temp().filePath("crossnet_batch_" + uniqueId);
    QDir().mkpath(tempDir);

    // 将文件复制到临时目录（保持原始文件名）
    QStringList tempFilePaths;
    for (const auto& fileInfo : files) {
        QString targetPath = QDir(tempDir).filePath(fileInfo.fileName);
        if (QFile::copy(fileInfo.tempPath, targetPath)) {
            tempFilePaths.append(targetPath);
        }
    }

    QString zipPath = QDir::temp().filePath("files_" + uniqueId + ".zip");

    // 使用系统命令创建 ZIP
    QProcess zipProcess;
    QStringList zipArgs;

#ifdef Q_OS_WIN
    // Windows 使用 PowerShell 的 Compress-Archive
    zipArgs << "-NoProfile" << "-Command";
    QString psCmd = "Compress-Archive -Path '" + tempDir.replace("'", "''") + "\\*' -DestinationPath '" + zipPath.replace("'", "''") + "' -Force";
    zipArgs << psCmd;
    zipProcess.start("powershell.exe", zipArgs);
#else
    // Linux/Mac 使用 zip 命令
    zipArgs << "-j" << zipPath;  // -j 不保留目录结构
    zipArgs << tempFilePaths;
    zipProcess.start("zip", zipArgs);
#endif

    if (!zipProcess.waitForFinished(30000) || zipProcess.exitCode() != 0) {
        // 清理临时目录
        QDir(tempDir).removeRecursively();
        for (const auto& fileInfo : files) {
            QFile::remove(fileInfo.tempPath);
        }

        HttpResponse response;
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.body = "Failed to create ZIP archive";
        sendResponse(socket, response);
        return;
    }

    // 发送 ZIP 文件
    QFile zipFile(zipPath);
    if (zipFile.open(QIODevice::ReadOnly)) {
        HttpResponse response;
        response.headers["Content-Type"] = "application/zip";
        response.headers["Content-Disposition"] = "attachment; filename*=UTF-8''" + QString::fromLatin1(QUrl::toPercentEncoding("files_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss") + ".zip"));
        response.body = zipFile.readAll();
        zipFile.close();
        sendResponse(socket, response);

        // 清理所有临时文件
        QFile::remove(zipPath);
        QDir(tempDir).removeRecursively();
        for (const auto& fileInfo : files) {
            if (fileInfo.tempPath.startsWith(QDir::temp().path()) && !fileInfo.tempPath.startsWith(tempDir)) {
                QFile::remove(fileInfo.tempPath);
            }
        }
    } else {
        QDir(tempDir).removeRecursively();
        for (const auto& fileInfo : files) {
            QFile::remove(fileInfo.tempPath);
        }

        HttpResponse response;
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.body = "Failed to read ZIP archive";
        sendResponse(socket, response);
    }
}

void WebServer::processWatermarkGeneration(QTcpSocket* socket, const QString& tempDir, const QString& tempFilePath, const QString& username, const QString& suggestionText) {
    HttpResponse response;

    QString originalFileName = QFileInfo(tempFilePath).fileName();

    // 生成水印图片
    qDebug() << "[Watermark] Calling generateWatermarkedImages with:" << tempFilePath;
    WatermarkService::WatermarkResult watermarkResult = watermarkService_->generateWatermarkedImages(
        tempFilePath,
        tempDir,
        originalFileName,
        suggestionText
    );
    qDebug() << "[Watermark] generateWatermarkedImages returned, success:" << watermarkResult.success;

    // 清理原始临时文件
    QFile::remove(tempFilePath);

    if (!watermarkResult.success) {
        qDebug() << "[Watermark] Error:" << watermarkResult.error;
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        QJsonObject errorObj;
        errorObj["success"] = false;
        errorObj["error"] = watermarkResult.error;
        response.body = QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
        sendResponse(socket, response);
        QDir(tempDir).removeRecursively();
        return;
    }

    // 读取生成的 ZIP 文件（或返回图片列表）
    qDebug() << "[Watermark] Processing result, zipFilePath:" << watermarkResult.zipFilePath;

    // 检查是否使用ZIP模式
    if (!watermarkResult.zipFilePath.isEmpty()) {
        // ZIP模式：返回ZIP文件
        QFile file(watermarkResult.zipFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "[Watermark] ERROR: Failed to open ZIP file!";
            response.statusCode = 500;
            response.statusText = "Internal Server Error";
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = R"({"success":false,"error":"无法读取生成的ZIP文件"})";
            sendResponse(socket, response);
            QDir(tempDir).removeRecursively();
            return;
        }

        qDebug() << "[Watermark] Successfully opened ZIP file";
        QByteArray fileData = file.readAll();
        file.close();
        qDebug() << "[Watermark] Read" << fileData.size() << "bytes from ZIP file";

        // 清理临时目录
        QDir(tempDir).removeRecursively();

        // 返回ZIP文件
        response.statusCode = 200;
        response.statusText = "OK";
        response.headers["Content-Type"] = "application/zip";
        QFileInfo fileInfo(watermarkResult.zipFilePath);
        response.headers["Content-Disposition"] = "attachment; filename=\"" + fileInfo.fileName().toUtf8() + "\"";
        response.body = fileData;

        sendResponse(socket, response);
    } else {
        // 非ZIP模式：返回图片文件列表和会话ID
        // 将临时目录和文件列表存储起来，供后续下载使用
        QString sessionId = QString::number(QDateTime::currentMSecsSinceEpoch());
        watermarkSessions_[sessionId] = {tempDir, watermarkResult.generatedFiles};

        QJsonObject resultObj;
        resultObj["success"] = true;
        resultObj["mode"] = "individual";
        resultObj["sessionId"] = sessionId;

        QJsonArray filesArray;
        for (const QString& fileName : watermarkResult.generatedFiles) {
            filesArray.append(fileName);
        }
        resultObj["files"] = filesArray;

        response.statusCode = 200;
        response.statusText = "OK";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = QJsonDocument(resultObj).toJson(QJsonDocument::Compact);

        qDebug() << "[Watermark] Individual mode response:";
        qDebug() << "[Watermark]   sessionId:" << sessionId;
        qDebug() << "[Watermark]   files:" << watermarkResult.generatedFiles;
        qDebug() << "[Watermark]   tempDir:" << tempDir;
        qDebug() << "[Watermark]   JSON:" << response.body;

        sendResponse(socket, response);
    }

    // 记录审计日志
    QString filePath = tempFilePath; // 使用临时文件路径作为文件标识
    emit logMessage(QString("[Watermark] User '%1' generated watermark for '%2' (%3 files)")
        .arg(username, filePath, QString::number(watermarkResult.generatedFiles.size())));
}

void WebServer::handleContentSearch(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证
    QString username;
    if (!isAuthenticated(request, username)) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "Unauthorized"}});
        sendResponse(socket, response);
        return;
    }
    
    // 解析请求参数
    nlohmann::json requestData;
    try {
        if (!request.body.isEmpty()) {
            requestData = nlohmann::json::parse(request.body.constData(), 
                                               request.body.constData() + request.body.size());
        }
    } catch (const std::exception& e) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "Invalid JSON"}});
        sendResponse(socket, response);
        return;
    }
    
    std::string query = requestData.value("query", "");
    
    if (query.empty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = jsonResponse({{"success", false}, {"error", "Query parameter is required"}});
        sendResponse(socket, response);
        return;
    }
    
    // 广播搜索请求到所有客户端
    QString searchId = server_->broadcastContentSearch(QString::fromStdString(query));
    
    // 等待结果（最多3秒）
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(server_, &Server::contentSearchResultReceived, &loop, [&](const QString& sid, const QString&) {
        if (sid == searchId) {
            // 可以在这里检查是否所有客户端都响应了
            // 为简单起见，我们继续等待直到超时
        }
    });
    
    timeout.start(3000);  // 3秒超时
    loop.exec();
    
    // 获取聚合结果
    QList<ContentSearchResult> results = server_->getContentSearchResults(searchId);
    
    // 构建响应
    nlohmann::json responseData;
    responseData["success"] = true;
    responseData["query"] = query;
    responseData["totalResults"] = results.size();
    responseData["results"] = nlohmann::json::array();
    
    for (const auto& result : results) {
        nlohmann::json item;
        item["filename"] = result.filename;
        item["relativePath"] = result.relativePath;
        item["ownerClient"] = result.ownerClient;
        item["size"] = result.size;
        
        // 格式化时间戳为可读格式 "YYYY-MM-DD HH:MM:SS"
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(result.modifyTime);
        item["modifyTime"] = dateTime.toString("yyyy-MM-dd HH:mm:ss").toStdString();
        
        responseData["results"].push_back(item);
    }
    
    // 清理缓存
    server_->clearContentSearchResults(searchId);
    
    HttpResponse response;
    response.statusCode = 200;
    response.statusText = "OK";
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.body = jsonResponse(responseData);
    
    sendResponse(socket, response);
    
    emit logMessage(QString("[HTTP] Content search request (client-only feature): %1")
        .arg(QString::fromStdString(query)));
}

}  // namespace CrossNetShare
