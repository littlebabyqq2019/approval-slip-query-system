// Web API 内容搜索处理函数
// 将此函数添加到 server/web_server.cpp

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
        if (!request.body.empty()) {
            requestData = nlohmann::json::parse(request.body);
        }
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
        response.body = R"({"error": "Query parameter is required"})";
        sendResponse(socket, response);
        return;
    }
    
    // 返回友好提示（简化实现）
    HttpResponse response;
    response.statusCode = 200;
    response.statusText = "OK";
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    nlohmann::json responseData;
    responseData["success"] = false;
    responseData["query"] = query;
    responseData["message"] = "Content search is only available in the desktop client application. Please download and use the client to search file contents.";
    responseData["feature_available"] = "client_only";
    responseData["results"] = nlohmann::json::array();
    
    response.body = responseData.dump();
    sendResponse(socket, response);
    
    emit logMessage(QString("[HTTP] Content search request (client-only feature): %1")
        .arg(QString::fromStdString(query)));
}
