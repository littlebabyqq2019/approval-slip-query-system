# Web 搜索最终补丁

## 已完成的修改

### 1. server/server.h ✅
- 添加搜索缓存结构
- 添加 `broadcastContentSearch()` 方法
- 添加 `getContentSearchResults()` 方法
- 添加 `clearContentSearchResults()` 方法
- 添加 `onContentSearchResponse()` 槽
- 添加 `contentSearchResultReceived` 信号

### 2. server/server.cpp ✅
- 实现搜索广播和结果聚合逻辑
- 添加必要的头文件

### 3. server/client_handler.h ✅
- 添加 `contentSearchResponse` 信号
- 添加 `handleContentSearchResponse()` 方法声明

### 4. server/client_handler.cpp ✅
- 在 switch 中添加 `CONTENT_SEARCH_RESPONSE` case
- 实现 `handleContentSearchResponse()` 方法
- 连接信号到 Server

### 5. client/client.h ✅
- 添加 `handleContentSearchRequest()` 方法声明
- 添加 `performContentSearch()` 方法声明

### 6. client/client.cpp ✅
- 在 switch 中添加 `CONTENT_SEARCH_REQUEST` case
- 实现 `handleContentSearchRequest()` 方法
- 实现 `performContentSearch()` 占位符

## 需要完成的修改

### 7. client/client.h - 添加 FileIndexer 引用
```cpp
class FileIndexer;  // 前向声明

class Client : public QObject {
    Q_OBJECT

public:
    // ... 现有方法 ...
    
    // 设置内容索引器（用于搜索）
    void setFileIndexer(FileIndexer* indexer);

private:
    // ... 现有成员 ...
    FileIndexer* fileIndexer_;  // 添加此成员
```

### 8. client/client.cpp - 实现 setFileIndexer 和 performContentSearch
```cpp
// 在构造函数中初始化
Client::Client(QObject* parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
    , connected_(false)
    , fileWatcher_(new QFileSystemWatcher(this))
    , refreshTimer_(new QTimer(this))
    , reconnectTimer_(new QTimer(this))
    , autoReconnect_(false)
    , reconnectAttempts_(0)
    , fileIndexer_(nullptr)  // 添加此行
{
    // ... 现有代码 ...
}

// 在文件末尾添加
void Client::setFileIndexer(FileIndexer* indexer) {
    fileIndexer_ = indexer;
}

QStringList Client::performContentSearch(const QString& query) {
    if (!fileIndexer_) {
        emit logMessage("[Client] FileIndexer not set, cannot perform content search");
        return QStringList();
    }
    
    // 调用 FileIndexer 进行搜索
    return fileIndexer_->search(query);
}
```

### 9. client/ui/main_window.cpp - 设置 FileIndexer
在 MainWindow 构造函数或 initializeIndexer() 方法中：
```cpp
void MainWindow::initializeIndexer() {
    // ... 现有的索引器初始化代码 ...
    
    // 设置 Client 的 FileIndexer 引用
    if (client_ && indexer_) {
        client_->setFileIndexer(indexer_);
        logMessage("[MainWindow] FileIndexer linked to Client for content search");
    }
}
```

### 10. server/web_server.cpp - 添加搜索 API 端点
```cpp
void WebServer::handleApiRequest(QHttpServerRequest&& request, QHttpServerResponder&& responder) {
    QString path = request.url().path();
    
    // ... 现有的 API 处理 ...
    
    // 内容搜索
    if (path == "/api/content/search" && request.method() == QHttpServerRequest::Method::Get) {
        handleContentSearchRequest(std::move(request), std::move(responder));
        return;
    }
    
    // ... 其他路由 ...
}

void WebServer::handleContentSearchRequest(QHttpServerRequest&& request, QHttpServerResponder&& responder) {
    // 检查认证
    if (!verifyAuth(request, responder)) {
        return;
    }
    
    // 获取查询参数
    QString query = request.query().queryItemValue("q");
    
    if (query.isEmpty()) {
        sendJsonResponse(responder, {
            {"success", false},
            {"error", "Query parameter 'q' is required"}
        }, 400);
        return;
    }
    
    // 广播搜索请求
    QString searchId = server_->broadcastContentSearch(query);
    
    // 等待结果（最多3秒）
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    int receivedCount = 0;
    
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(server_, &Server::contentSearchResultReceived, [&](const QString& sid, const QString&) {
        if (sid == searchId) {
            receivedCount++;
            // 可以设置收到所有客户端响应后立即返回
        }
    });
    
    timeout.start(3000);  // 3秒超时
    loop.exec();
    
    // 获取聚合结果
    QList<ContentSearchResult> results = server_->getContentSearchResults(searchId);
    
    // 构建响应
    nlohmann::json response;
    response["success"] = true;
    response["query"] = query.toStdString();
    response["totalResults"] = results.size();
    response["results"] = nlohmann::json::array();
    
    for (const auto& result : results) {
        nlohmann::json item;
        item["filename"] = result.filename;
        item["relativePath"] = result.relativePath;
        item["ownerClient"] = result.ownerClient;
        item["size"] = result.size;
        item["modifyTime"] = result.modifyTime;
        response["results"].push_back(item);
    }
    
    // 清理缓存
    server_->clearContentSearchResults(searchId);
    
    sendJsonResponse(responder, response);
}
```

### 11. server/web_server.h - 添加方法声明
```cpp
private:
    // ... 现有方法 ...
    void handleContentSearchRequest(QHttpServerRequest&& request, QHttpServerResponder&& responder);
```

## 编译和测试

### 编译
```bash
cd build
cmake --build . --config Release
```

### 测试步骤

1. **启动服务器**
   ```
   CrossNetShareServer.exe
   ```

2. **启动客户端** （至少2个）
   ```
   CrossNetShareClient.exe
   ```

3. **Web 搜索测试**
   ```
   http://localhost:8080/api/content/search?q=雁塔
   ```

### 预期结果
```json
{
  "success": true,
  "query": "雁塔",
  "totalResults": 4,
  "results": [
    {
      "filename": "2026-文件批办单#6600-(B-26-2073).doc",
      "relativePath": "2026-文件批办单#6600-(B-26-2073).doc",
      "ownerClient": "ClientA",
      "size": 123456,
      "modifyTime": 1704067200
    },
    // ... 更多结果
  ]
}
```

## 时间估算

- 步骤 7-9（Client 集成）：10 分钟
- 步骤 10-11（Web API）：15 分钟
- 测试和调试：10 分钟

**总计**：约 35 分钟

## 当前状态

- ✅ 服务器端消息处理完成
- ✅ 客户端端消息处理完成（需要连接 FileIndexer）
- ⏳ Client-FileIndexer 集成
- ⏳ Web API 端点

## 下一步

选择以下之一：

**选项 A：现在完成全部**（35分钟）
- 立即实现步骤 7-11
- 完整测试
- 发布 v2.0.0（包含 Web 搜索）

**选项 B：分步发布**
- 先提交当前代码（服务器和客户端消息处理）
- 发布 v2.0.0（客户端搜索）
- 在 v2.1.0 中完成 Web 搜索

**我的建议**：选项 A，因为已经完成了 80%，再花 35 分钟就能完整实现。

请告诉我你的选择！
