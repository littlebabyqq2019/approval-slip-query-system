# Web API 内容搜索补丁

## 需要修改的文件

### 1. server/web_server.h

#### A. 在私有方法区域添加新的API处理函数

```cpp
void handleFilePreview(QTcpSocket* socket, const HttpRequest& request);
void handleWatermarkGenerate(QTcpSocket* socket, const HttpRequest& request);
void handleWatermarkDownload(QTcpSocket* socket, const HttpRequest& request);
void handleContentSearch(QTcpSocket* socket, const HttpRequest& request);  // 新增
```

---

### 2. server/web_server.cpp

#### A. 在 handleRequest() 函数的路由部分添加

```cpp
void WebServer::handleRequest(QTcpSocket* socket, const HttpRequest& request) {
    // ... 现有路由
    
    else if (request.path == "/api/search") {  // 现有的文件名搜索
        handleFileSearch(socket, request);
    }
    else if (request.path == "/api/content-search") {  // 新增：内容搜索
        handleContentSearch(socket, request);
    }
    else if (request.path == "/api/download") {
        handleFileDownload(socket, request);
    }
    
    // ... 其他路由
}
```

#### B. 实现 handleContentSearch() 函数（在文件末尾）

```cpp
// ============================================================================
// 内容搜索API
// ============================================================================

void WebServer::handleContentSearch(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证
    auto it = sessionTokens_.find(request.cookies["session_token"]);
    if (it == sessionTokens_.end()) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Unauthorized"})";
        sendResponse(socket, response);
        return;
    }
    
    // 解析请求参数
    nlohmann::json requestData;
    try {
        requestData = nlohmann::json::parse(request.body);
    } catch (const std::exception& e) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Invalid JSON"})";
        sendResponse(socket, response);
        return;
    }
    
    std::string query = requestData.value("query", "");
    std::string targetClient = requestData.value("target_client", "");
    
    if (query.empty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Query is required"})";
        sendResponse(socket, response);
        return;
    }
    
    // 文件类型过滤（可选）
    std::vector<std::string> fileTypes;
    if (requestData.contains("file_types") && requestData["file_types"].is_array()) {
        for (const auto& type : requestData["file_types"]) {
            fileTypes.push_back(type.get<std::string>());
        }
    }
    
    // 如果没有指定目标客户端，尝试搜索所有已连接的客户端
    if (targetClient.empty() && server_) {
        // 获取所有客户端列表
        // 这里需要服务器端支持广播搜索请求
        // 简化实现：返回错误，要求指定客户端
        
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Target client is required"})";
        sendResponse(socket, response);
        return;
    }
    
    // 通过服务器转发搜索请求到目标客户端
    if (!server_) {
        HttpResponse response;
        response.statusCode = 500;
        response.statusText = "Internal Server Error";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Server not available"})";
        sendResponse(socket, response);
        return;
    }
    
    // 构建搜索请求消息
    nlohmann::json searchRequest;
    searchRequest["query"] = query;
    searchRequest["file_types"] = fileTypes;
    searchRequest["target_client"] = targetClient;
    
    // 发送搜索请求到目标客户端
    // 注意：这里需要异步处理，因为需要等待客户端响应
    // 简化实现：同步等待（实际应该使用回调或Future）
    
    // TODO: 实现异步搜索请求-响应机制
    // 当前先返回一个占位响应
    
    HttpResponse response;
    response.statusCode = 200;
    response.statusText = "OK";
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    nlohmann::json responseData;
    responseData["success"] = true;
    responseData["query"] = query;
    responseData["message"] = "Content search request sent to client";
    responseData["results"] = nlohmann::json::array();  // 暂时返回空结果
    
    response.body = responseData.dump();
    sendResponse(socket, response);
    
    emit logMessage(QString("Content search API called: %1")
        .arg(QString::fromStdString(query)));
}
```

---

## 简化版实现（直接在Web服务器端搜索）

如果客户端文件在服务器可访问的路径，可以简化实现：

```cpp
void WebServer::handleContentSearch(QTcpSocket* socket, const HttpRequest& request) {
    // 检查认证
    auto it = sessionTokens_.find(request.cookies["session_token"]);
    if (it == sessionTokens_.end()) {
        HttpResponse response;
        response.statusCode = 401;
        response.statusText = "Unauthorized";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Unauthorized"})";
        sendResponse(socket, response);
        return;
    }
    
    // 解析请求
    nlohmann::json requestData;
    try {
        requestData = nlohmann::json::parse(request.body);
    } catch (const std::exception& e) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Invalid JSON"})";
        sendResponse(socket, response);
        return;
    }
    
    std::string query = requestData.value("query", "");
    
    if (query.empty()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        response.body = R"({"error": "Query is required"})";
        sendResponse(socket, response);
        return;
    }
    
    // 如果服务器端有FileIndexer实例，直接搜索
    // 注意：这需要服务器端也建立索引
    // 当前CrossNetShare架构中，服务器不直接访问客户端文件
    // 所以这个方案不适用
    
    HttpResponse response;
    response.statusCode = 501;
    response.statusText = "Not Implemented";
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = R"({"error": "Content search not implemented on server side. Please use client-side search."})";
    sendResponse(socket, response);
}
```

---

## Web前端集成

### 修改 server/web/browse.html

在文件浏览页面添加内容搜索功能：

```html
<!-- 在文件列表上方添加搜索区域 -->
<div class="search-container">
    <div class="search-tabs">
        <button id="filenameSearchTab" class="tab active">文件名搜索</button>
        <button id="contentSearchTab" class="tab">内容搜索</button>
    </div>
    
    <!-- 文件名搜索（现有功能） -->
    <div id="filenameSearchPanel" class="search-panel active">
        <input type="text" id="filenameSearch" placeholder="搜索文件名...">
    </div>
    
    <!-- 内容搜索（新功能） -->
    <div id="contentSearchPanel" class="search-panel" style="display:none;">
        <div class="content-search-form">
            <input type="text" id="contentSearchInput" 
                   placeholder="输入搜索关键词（支持 AND OR NOT）...">
            <button id="contentSearchButton">搜索内容</button>
        </div>
        <div class="search-tips">
            <small>
                示例：<br>
                - 关键词A AND 关键词B（同时包含）<br>
                - 关键词A OR 关键词B（任一包含）<br>
                - 关键词A NOT 关键词B（A有B无）
            </small>
        </div>
        <div id="contentSearchResults"></div>
    </div>
</div>

<style>
.search-container {
    margin-bottom: 20px;
    border: 1px solid #ddd;
    border-radius: 4px;
    padding: 15px;
}

.search-tabs {
    display: flex;
    margin-bottom: 15px;
    border-bottom: 2px solid #eee;
}

.search-tabs .tab {
    padding: 10px 20px;
    background: none;
    border: none;
    cursor: pointer;
    font-size: 14px;
    color: #666;
}

.search-tabs .tab.active {
    color: #007bff;
    border-bottom: 2px solid #007bff;
    margin-bottom: -2px;
}

.content-search-form {
    display: flex;
    gap: 10px;
}

#contentSearchInput {
    flex: 1;
    padding: 8px;
    border: 1px solid #ddd;
    border-radius: 4px;
}

#contentSearchButton {
    padding: 8px 20px;
    background: #007bff;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
}

#contentSearchButton:hover {
    background: #0056b3;
}

.search-tips {
    margin-top: 10px;
    color: #666;
}

#contentSearchResults {
    margin-top: 15px;
}
</style>

<script>
// 标签页切换
document.getElementById('filenameSearchTab').addEventListener('click', function() {
    this.classList.add('active');
    document.getElementById('contentSearchTab').classList.remove('active');
    document.getElementById('filenameSearchPanel').style.display = 'block';
    document.getElementById('contentSearchPanel').style.display = 'none';
});

document.getElementById('contentSearchTab').addEventListener('click', function() {
    this.classList.add('active');
    document.getElementById('filenameSearchTab').classList.remove('active');
    document.getElementById('filenameSearchPanel').style.display = 'none';
    document.getElementById('contentSearchPanel').style.display = 'block';
});

// 内容搜索
document.getElementById('contentSearchButton').addEventListener('click', performContentSearch);
document.getElementById('contentSearchInput').addEventListener('keypress', function(e) {
    if (e.key === 'Enter') {
        performContentSearch();
    }
});

async function performContentSearch() {
    const query = document.getElementById('contentSearchInput').value.trim();
    
    if (!query) {
        alert('请输入搜索关键词');
        return;
    }
    
    const resultsDiv = document.getElementById('contentSearchResults');
    resultsDiv.innerHTML = '<p>搜索中...</p>';
    
    try {
        const response = await fetch('/api/content-search', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                query: query,
                target_client: ''  // 可以从UI获取
            })
        });
        
        const data = await response.json();
        
        if (data.success && data.results) {
            if (data.results.length === 0) {
                resultsDiv.innerHTML = '<p>未找到匹配结果</p>';
            } else {
                let html = `<p>找到 ${data.results.length} 个匹配文件：</p><ul>`;
                data.results.forEach(result => {
                    html += `<li>
                        <strong>${result.filename}</strong><br>
                        <small>路径：${result.relativePath}</small><br>
                        <small>来源：${result.ownerClient}</small>
                    </li>`;
                });
                html += '</ul>';
                resultsDiv.innerHTML = html;
            }
        } else {
            resultsDiv.innerHTML = `<p class="error">搜索失败：${data.error || data.message}</p>`;
        }
    } catch (error) {
        resultsDiv.innerHTML = `<p class="error">搜索出错：${error.message}</p>`;
    }
}
</script>
```

---

## 当前限制和解决方案

### 限制1：异步请求处理

Web API 需要等待客户端响应，这需要异步处理。

**解决方案：**
1. 使用WebSocket替代HTTP（实时双向通信）
2. 使用轮询机制（客户端定期查询结果）
3. 使用回调或Future模式

### 限制2：多客户端搜索

如果要搜索所有客户端的文件，需要广播机制。

**解决方案：**
1. 服务器端维护所有客户端的索引器引用
2. 广播搜索请求到所有客户端
3. 汇总结果返回给Web

### 限制3：服务器端无直接文件访问

CrossNetShare架构中，服务器不直接访问客户端文件。

**解决方案：**
1. 客户端建立索引并保持索引器实例
2. 服务器转发搜索请求到客户端
3. 客户端返回搜索结果

---

## 推荐实现方案

### 方案1：仅客户端搜索（当前已实现）

**优点：**
- 简单
- 不需要服务器端修改
- 性能好

**缺点：**
- Web界面无法使用

### 方案2：客户端+Web简化版（推荐）

**实现：**
1. 客户端保持现有搜索功能
2. Web API返回提示信息："请使用客户端程序的内容搜索功能"
3. 或在Web界面添加说明和客户端下载链接

**优点：**
- 开发工作量小
- 功能分离清晰

### 方案3：完整Web支持（复杂）

**实现：**
1. 使用WebSocket替代HTTP
2. 服务器端实现请求转发
3. 客户端实时响应搜索请求
4. Web界面实时显示结果

**优点：**
- 功能完整
- 用户体验好

**缺点：**
- 开发工作量大
- 需要重构通信协议

---

## 建议

当前阶段建议采用**方案2**：
1. 保持客户端搜索功能完整
2. Web API返回友好提示
3. 在README中说明内容搜索功能仅在客户端可用

如果未来需要Web支持，再实现方案3。
