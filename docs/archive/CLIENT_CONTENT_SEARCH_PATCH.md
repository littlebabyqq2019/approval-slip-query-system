# 客户端内容搜索协议补丁

## 需要修改的文件

### 1. client/client.h

#### A. 在公共方法区域添加（在 uploadFile 之后）

```cpp
// 上传文件
void uploadFile(const QString& localPath, const QString& relativePath);

// 内容搜索 - 新增
void requestContentSearch(const QString& query, const QStringList& fileTypes = QStringList());
```

#### B. 在信号区域添加（在 logMessage 之后）

```cpp
void logMessage(const QString& message);

// 内容搜索结果 - 新增
void contentSearchResults(const std::vector<ContentSearchResult>& results, const QString& query);
```

#### C. 在私有槽函数区域添加

```cpp
private slots:
    // ... 现有槽函数
    
    void onContentSearchReceived(const QString& query, const QStringList& fileTypes);  // 新增
```

#### D. 在私有方法区域添加（handleMessage 之后）

```cpp
void handleBatchDownloadResponse(const nlohmann::json& payload);
void handleContentSearchRequest(const nlohmann::json& payload);  // 新增
void handleContentSearchResponse(const nlohmann::json& payload); // 新增
void handleErrorMessage(const nlohmann::json& payload);
```

---

### 2. client/client.cpp

#### A. 在文件开头添加头文件

```cpp
#include "client.h"
#include "file_indexer.h"  // 新增
#include "common/protocol.h"
```

#### B. 在构造函数中初始化（如果需要本地索引器）

```cpp
Client::Client(QObject* parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
    , connected_(false)
    , reconnectTimer_(new QTimer(this))
    , autoReconnect_(false)
    , serverPort_(0)
    , reconnectAttempts_(0)
    , fileWatcher_(new QFileSystemWatcher(this))
    , refreshTimer_(new QTimer(this))
    , indexer_(nullptr)  // 新增（如果客户端需要直接访问索引器）
{
    // ... 现有代码
}
```

#### C. 添加 handleMessage 分支（在 switch 语句中）

```cpp
void Client::handleMessage(MessageType type, const nlohmann::json& payload) {
    switch (type) {
    // ... 现有 case 语句
    
    case MessageType::CONTENT_SEARCH_REQUEST:  // 新增
        handleContentSearchRequest(payload);
        break;
        
    case MessageType::CONTENT_SEARCH_RESPONSE:  // 新增
        handleContentSearchResponse(payload);
        break;
        
    // ... 其他 case
    }
}
```

#### D. 实现新的方法（在文件末尾）

```cpp
// ============================================================================
// 内容搜索
// ============================================================================

void Client::requestContentSearch(const QString& query, const QStringList& fileTypes) {
    if (!connected_) {
        emit error("Not connected to server");
        return;
    }
    
    nlohmann::json payload;
    payload["query"] = query.toStdString();
    
    if (!fileTypes.isEmpty()) {
        std::vector<std::string> types;
        for (const QString& type : fileTypes) {
            types.push_back(type.toStdString());
        }
        payload["file_types"] = types;
    }
    
    payload["target_client"] = "";  // 空字符串表示当前客户端
    
    sendMessage(MessageType::CONTENT_SEARCH_REQUEST, payload);
    emit logMessage(QString("Content search request sent: %1").arg(query));
}

void Client::handleContentSearchRequest(const nlohmann::json& payload) {
    try {
        std::string query = payload.value("query", "");
        
        QStringList fileTypes;
        if (payload.contains("file_types")) {
            for (const auto& type : payload["file_types"]) {
                fileTypes << QString::fromStdString(type.get<std::string>());
            }
        }
        
        emit logMessage(QString("Received content search request: %1")
            .arg(QString::fromStdString(query)));
        
        // 触发槽函数处理搜索（让主窗口或其他组件处理）
        emit onContentSearchReceived(QString::fromStdString(query), fileTypes);
        
    } catch (const std::exception& e) {
        emit error(QString("Failed to parse content search request: %1").arg(e.what()));
    }
}

void Client::handleContentSearchResponse(const nlohmann::json& payload) {
    try {
        std::string query = payload.value("query", "");
        bool success = payload.value("success", false);
        
        if (!success) {
            std::string errorMsg = payload.value("error", "Unknown error");
            emit error(QString("Content search failed: %1")
                .arg(QString::fromStdString(errorMsg)));
            return;
        }
        
        std::vector<ContentSearchResult> results;
        
        if (payload.contains("results")) {
            for (const auto& item : payload["results"]) {
                ContentSearchResult result;
                result.filename = item.value("filename", "");
                result.relativePath = item.value("relative_path", "");
                result.ownerClient = item.value("owner_client", "");
                result.size = item.value("size", 0);
                result.modifyTime = item.value("modify_time", 0);
                
                results.push_back(result);
            }
        }
        
        emit logMessage(QString("Content search completed: %1 results for '%2'")
            .arg(results.size())
            .arg(QString::fromStdString(query)));
        
        emit contentSearchResults(results, QString::fromStdString(query));
        
    } catch (const std::exception& e) {
        emit error(QString("Failed to parse content search response: %1").arg(e.what()));
    }
}
```

---

### 3. 在主窗口中处理内容搜索

#### client/ui/main_window.h

在私有槽函数区域添加：

```cpp
private slots:
    // ... 现有槽函数
    
    // 内容搜索相关 - 新增
    void onContentSearchRequested(const QString& query, const QStringList& fileTypes);
    void onContentSearchResultsReceived(const std::vector<ContentSearchResult>& results, const QString& query);
```

#### client/ui/main_window.cpp

在构造函数中连接信号：

```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    // ... 其他初始化
{
    // ... 现有代码
    
    // 连接内容搜索信号 - 新增
    connect(client_, &Client::contentSearchResults, 
            this, &MainWindow::onContentSearchResultsReceived);
}
```

修改 `performContentSearch` 函数使用客户端协议：

```cpp
void MainWindow::performContentSearch(const QString& query) {
    searchResultLabel_->setText("搜索中...");
    searchResultLabel_->setStyleSheet("color: orange;");
    
    if (indexer_) {
        // 本地搜索（如果有索引器）
        QtConcurrent::run([this, query]() {
            QStringList results = indexer_->search(query);
            
            QMetaObject::invokeMethod(this, [this, results, query]() {
                displaySearchResults(results, query);
            }, Qt::QueuedConnection);
        });
    } else {
        // 通过服务器搜索（如果没有本地索引器）
        QStringList fileTypes;
        // 可以从UI获取文件类型过滤
        client_->requestContentSearch(query, fileTypes);
    }
}

void MainWindow::onContentSearchResultsReceived(
    const std::vector<ContentSearchResult>& results, const QString& query) {
    
    if (results.empty()) {
        searchResultLabel_->setText("未找到匹配结果");
        searchResultLabel_->setStyleSheet("color: gray;");
        onLogMessage(QString("Content search for '%1': no results").arg(query));
    } else {
        searchResultLabel_->setText(QString("找到 %1 个匹配文件").arg(results.size()));
        searchResultLabel_->setStyleSheet("color: green;");
        onLogMessage(QString("Content search for '%1': %2 results")
            .arg(query).arg(results.size()));
        
        // 显示结果
        onLogMessage("Search results:");
        for (size_t i = 0; i < results.size() && i < 20; ++i) {
            onLogMessage(QString("  - %1 (%2)")
                .arg(QString::fromStdString(results[i].relativePath))
                .arg(QString::fromStdString(results[i].ownerClient)));
        }
        if (results.size() > 20) {
            onLogMessage(QString("  ... and %1 more results").arg(results.size() - 20));
        }
    }
}
```

---

## 完成后的功能流程

### 流程1：本地搜索（客户端有索引器）
```
用户输入 → 主窗口 → 本地索引器 → 返回结果 → 显示
```

### 流程2：远程搜索（通过服务器）
```
用户输入 → 主窗口 → 客户端 → 服务器 → 目标客户端 → 索引器 → 返回结果 → 显示
```

这样即使客户端没有本地文件，也可以通过服务器搜索其他客户端的文件。

---

## 下一步

完成这些修改后，继续实现服务器端的转发逻辑。
