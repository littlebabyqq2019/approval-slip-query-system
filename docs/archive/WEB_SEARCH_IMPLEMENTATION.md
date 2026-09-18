# Web 全文搜索实现方案

## 📋 需求分析

**目标**：让 Web 用户能够搜索所有已连接客户端的文件内容

**挑战**：
- 服务器没有索引数据库
- 索引数据分散在各个客户端
- 需要实时聚合结果

## 🏗️ 架构设计

### 方案选择

**方案 A：服务器维护中心索引** ❌
- 优点：快速响应
- 缺点：需要同步所有客户端数据，复杂度高

**方案 B：广播搜索请求** ✅ **采用此方案**
- 优点：无需同步，实时准确
- 缺点：响应稍慢（需要等待客户端）

### 流程图

```
Web用户                服务器                  客户端A               客户端B
  |                      |                       |                     |
  |--搜索"雁塔"--------->|                       |                     |
  |                      |--广播搜索请求-------->|                     |
  |                      |--广播搜索请求----------------------->|
  |                      |                       |                     |
  |                      |<--返回结果(2个文件)---|                     |
  |                      |<--返回结果(1个文件)-------------------------|
  |                      |                       |                     |
  |<--返回聚合结果(3个)--|                       |                     |
  |                      |                       |                     |
```

## 🔧 实现步骤

### 步骤 1：扩展消息协议（已完成）

`common/message.h` 已包含：
```cpp
CONTENT_SEARCH_REQUEST,
CONTENT_SEARCH_RESPONSE,
```

### 步骤 2：服务器端实现

#### 2.1 在 `server/server.h` 添加

```cpp
// 内容搜索
void broadcastContentSearchRequest(const QString& query);
QList<ContentSearchResult> getContentSearchResults(const QString& query);

private:
    struct ContentSearchResult {
        QString clientId;
        QString filePath;
        QString fileName;
    };
    
    QMap<QString, QList<ContentSearchResult>> contentSearchCache_;
    QMutex searchCacheMutex_;
```

#### 2.2 在 `server/server.cpp` 实现

```cpp
void Server::broadcastContentSearchRequest(const QString& query) {
    // 清除旧的搜索结果
    QMutexLocker locker(&searchCacheMutex_);
    contentSearchCache_.clear();
    
    // 创建搜索请求
    nlohmann::json payload;
    payload["query"] = query.toStdString();
    
    QByteArray message = Protocol::serializeMessage(
        MessageType::CONTENT_SEARCH_REQUEST, 
        payload
    );
    
    // 广播到所有客户端
    for (auto* client : clients_) {
        if (client && client->isConnected()) {
            client->socket()->write(message);
            client->socket()->flush();
        }
    }
    
    emit logMessage(QString("[Server] Broadcasted content search: %1").arg(query));
}

QList<Server::ContentSearchResult> Server::getContentSearchResults(const QString& query) {
    QMutexLocker locker(&searchCacheMutex_);
    return contentSearchCache_.value(query);
}

// 在 handleMessage() 中添加处理
void Server::handleMessage(ClientHandler* client, MessageType type, const nlohmann::json& payload) {
    // ... 现有代码 ...
    
    case MessageType::CONTENT_SEARCH_RESPONSE: {
        QString query = QString::fromStdString(payload["query"]);
        auto results = payload["results"];
        
        QMutexLocker locker(&searchCacheMutex_);
        for (const auto& item : results) {
            ContentSearchResult result;
            result.clientId = client->clientId();
            result.filePath = QString::fromStdString(item["file_path"]);
            result.fileName = QString::fromStdString(item["file_name"]);
            contentSearchCache_[query].append(result);
        }
        
        emit logMessage(QString("[Server] Received %1 search results from %2")
            .arg(results.size()).arg(client->clientId()));
        break;
    }
}
```

### 步骤 3：客户端实现

#### 3.1 在 `client/client.cpp` 的 handleMessage() 添加

```cpp
case MessageType::CONTENT_SEARCH_REQUEST: {
    QString query = QString::fromStdString(payload["query"]);
    emit logMessage(QString("[Client] Received content search request: %1").arg(query));
    
    // 执行搜索
    if (fileIndexer_) {
        QStringList results = fileIndexer_->search(query);
        
        // 构建响应
        nlohmann::json response;
        response["query"] = query.toStdString();
        response["results"] = nlohmann::json::array();
        
        for (const QString& filePath : results) {
            QFileInfo fileInfo(filePath);
            nlohmann::json item;
            item["file_path"] = filePath.toStdString();
            item["file_name"] = fileInfo.fileName().toStdString();
            response["results"].push_back(item);
        }
        
        // 发送响应
        QByteArray message = Protocol::serializeMessage(
            MessageType::CONTENT_SEARCH_RESPONSE,
            response
        );
        socket_->write(message);
        socket_->flush();
        
        emit logMessage(QString("[Client] Sent %1 search results").arg(results.size()));
    }
    break;
}
```

### 步骤 4：Web API 实现

在 `server/web_server.cpp` 的 `handleContentSearch()` 修改：

```cpp
void WebServer::handleContentSearch(QTcpSocket* socket, const HttpRequest& request) {
    // ... 认证代码保持不变 ...
    
    std::string query;
    auto queryIt = request.queryParams.find("q");
    if (queryIt != request.queryParams.end()) {
        query = queryIt->second;
    }
    
    if (query.empty()) {
        sendJsonResponse(socket, 400, {
            {"success", false},
            {"message", "搜索关键词不能为空"}
        });
        return;
    }
    
    emit logMessage(QString("[HTTP] Content search query: %1")
        .arg(QString::fromStdString(query)));
    
    // 向服务器广播搜索请求
    if (server_) {
        server_->broadcastContentSearchRequest(QString::fromStdString(query));
        
        // 等待客户端响应（简单实现）
        QThread::msleep(500);  // 等待500ms
        
        // 获取搜索结果
        auto results = server_->getContentSearchResults(QString::fromStdString(query));
        
        nlohmann::json responseData;
        responseData["success"] = true;
        responseData["query"] = query;
        responseData["results"] = nlohmann::json::array();
        
        for (const auto& result : results) {
            nlohmann::json item;
            item["client_id"] = result.clientId.toStdString();
            item["file_path"] = result.filePath.toStdString();
            item["file_name"] = result.fileName.toStdString();
            responseData["results"].push_back(item);
        }
        
        responseData["total"] = results.size();
        
        sendJsonResponse(socket, 200, responseData);
        
        emit logMessage(QString("[HTTP] Content search returned %1 results")
            .arg(results.size()));
    } else {
        sendJsonResponse(socket, 500, {
            {"success", false},
            {"message", "服务器内部错误"}
        });
    }
}
```

## ⚡ 性能优化

### 当前实现（简单版）
- 固定等待 500ms
- 适合小规模部署（< 10 个客户端）

### 未来优化
1. **动态等待**：根据客户端数量调整等待时间
2. **异步响应**：使用 WebSocket 推送结果
3. **超时机制**：设置最大等待时间
4. **结果缓存**：相同查询短时间内返回缓存

## 🧪 测试计划

### 单元测试

**测试 1：单客户端搜索**
```
客户端A：有"雁塔"文件
Web搜索："雁塔"
预期：返回客户端A的结果
```

**测试 2：多客户端搜索**
```
客户端A：有"雁塔"文件2个
客户端B：有"雁塔"文件1个
Web搜索："雁塔"
预期：返回3个结果（A:2 + B:1）
```

**测试 3：布尔运算符**
```
Web搜索："雁 AND 塔"
预期：正确支持AND运算符
```

## 📝 API 文档

### GET /api/content-search

**请求参数**：
```
q: 搜索关键词（必需）
```

**响应示例**：
```json
{
  "success": true,
  "query": "雁塔",
  "total": 3,
  "results": [
    {
      "client_id": "ClientA",
      "file_path": "C:/path/to/file1.txt",
      "file_name": "file1.txt"
    },
    {
      "client_id": "ClientB",
      "file_path": "D:/path/to/file2.doc",
      "file_name": "file2.doc"
    }
  ]
}
```

## 🎯 实施优先级

**Phase 1（当前）**：
- ✅ 消息协议定义
- ⏳ 服务器广播实现
- ⏳ 客户端响应实现
- ⏳ Web API 实现

**Phase 2（v1.5.1）**：
- 结果缓存
- 超时处理
- 错误处理

**Phase 3（v1.6.0）**：
- WebSocket 实时推送
- 异步搜索
- 搜索历史

---

**预计实现时间**：30-45 分钟  
**测试时间**：10-15 分钟  
**总时间**：约 1 小时
