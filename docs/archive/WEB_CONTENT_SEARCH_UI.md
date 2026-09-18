# Web UI 全文搜索功能实现

## 更新日期
2026-09-01

## 问题描述
Web 页面的搜索框只支持客户端过滤（文件名/路径），不能进行全文内容搜索。用户在 Web 输入"雁塔"等关键词无法找到包含这些内容的文件。

## 解决方案

### 1. 修改搜索框 UI
- 添加了"全文搜索"按钮在搜索输入框旁边（绿色按钮）
- 更新搜索框占位符：`"文件名过滤 / 按回车全文搜索"`
- 添加了 CSS 样式用于高亮全文搜索结果

### 2. 双重搜索功能
#### 文件名过滤（实时）
- 输入时触发 `input` 事件
- 在前端过滤 `files` 数组
- 匹配文件名和相对路径（不区分大小写）
- 立即显示结果

#### 全文内容搜索
- 点击"全文搜索"按钮或按回车键触发
- 调用后端 API：`POST /api/content-search`
- 广播搜索请求到所有客户端
- 等待最多 3 秒收集结果
- 显示包含关键词的文件

### 3. 用户体验改进
- 全文搜索结果用黄色背景 + 橙色左边框高亮
- 文件名旁显示绿色"全文"徽章标识
- 搜索按钮在搜索时显示"搜索中..."并禁用
- 清空输入框时自动退出全文搜索模式，返回文件列表
- 客户端过滤器在全文搜索模式下仍然有效

### 4. 代码变更
**文件**: `server/web_server.cpp` - `serveBrowsePage()` 方法

#### CSS 新增样式
```css
.search button {
  border:0;
  border-radius:8px;
  padding:10px 16px;
  cursor:pointer;
  background:#10b981;
  color:white;
  font-size:14px;
  white-space:nowrap
}
.search button:disabled {
  opacity:0.5;
  cursor:not-allowed
}
.item.content-match {
  background:#fef3c7;
  border-left:3px solid #f59e0b
}
.badge {
  display:inline-block;
  background:#10b981;
  color:white;
  padding:2px 6px;
  border-radius:4px;
  font-size:11px;
  margin-left:6px
}
```

#### HTML 修改
```html
<div class="search">
  <input id="q" placeholder="文件名过滤 / 按回车全文搜索">
  <button id="contentSearchBtn" onclick="doContentSearch()">全文搜索</button>
</div>
```

#### JavaScript 新增功能
1. **新增状态变量**:
   ```javascript
   let contentSearchResults = [];
   let isContentSearchMode = false;
   ```

2. **新增全文搜索函数**:
   ```javascript
   async function doContentSearch() {
     const query = document.getElementById('q').value.trim();
     if (!query) {
       alert('请输入搜索关键词');
       return;
     }
     
     const btn = document.getElementById('contentSearchBtn');
     btn.disabled = true;
     btn.textContent = '搜索中...';
     
     try {
       const r = await fetch('/api/content-search', {
         method: 'POST',
         headers: { 'Content-Type': 'application/json' },
         body: JSON.stringify({ query: query })
       });
       
       if (r.status === 401) {
         location.href = '/login';
         return;
       }
       
       const j = await r.json();
       if (j.success) {
         contentSearchResults = j.results || [];
         isContentSearchMode = true;
         render();
       } else {
         alert('搜索失败: ' + (j.error || '未知错误'));
       }
     } catch (e) {
       alert('搜索失败: ' + e.message);
     } finally {
       btn.disabled = false;
       btn.textContent = '全文搜索';
     }
   }
   ```

3. **修改 render() 函数**:
   - 检查 `isContentSearchMode` 标志
   - 如果是全文搜索模式，显示 `contentSearchResults`
   - 否则显示过滤后的 `files` 列表
   - 为全文搜索结果添加 `isContentMatch` 标记和高亮样式

4. **事件处理改进**:
   ```javascript
   // 输入时实时过滤 + 退出全文搜索
   document.getElementById('q').addEventListener('input', () => {
     if (isContentSearchMode && document.getElementById('q').value.trim() === '') {
       isContentSearchMode = false;
       contentSearchResults = [];
     }
     render();
   });
   
   // 回车键触发全文搜索
   document.getElementById('q').addEventListener('keydown', (e) => {
     if (e.key === 'Enter') {
       doContentSearch();
     }
   });
   ```

## 使用说明

### 文件名过滤（实时）
1. 在搜索框输入关键词
2. 自动过滤显示匹配的文件
3. 清空输入框返回所有文件

### 全文搜索
1. 在搜索框输入搜索关键词（如"雁塔"）
2. 点击"全文搜索"按钮或按回车键
3. 等待搜索完成（最多 3 秒）
4. 查看全文搜索结果（带"全文"徽章和黄色高亮）
5. 清空输入框返回文件列表

### 搜索技巧
- 支持布尔运算符：`"雁塔 or 中国"`, `"西安 and 卫生"`, `"西安 not 雁塔"`
- 支持中文和英文搜索
- FTS5 全文索引 + LIKE 降级确保搜索可靠性

## 后端 API
API 端点：`POST /api/content-search`

请求体：
```json
{
  "query": "雁塔"
}
```

响应：
```json
{
  "success": true,
  "query": "雁塔",
  "totalResults": 4,
  "results": [
    {
      "filename": "文件1.docx",
      "relativePath": "Documents/文件1.docx",
      "ownerClient": "Client-001",
      "size": 12345,
      "modifyTime": "2026-08-25 10:30:00"
    }
  ]
}
```

## 技术细节
- 后端实现：`WebServer::handleContentSearch()` (server/web_server.cpp:1302)
- 搜索广播：`Server::broadcastContentSearch()`
- 客户端处理：`Client::handleContentSearchRequest()`
- 搜索实现：`FileIndexer::searchWithBoolean()` (支持 OR 混合策略)

## 测试验证
使用 PowerShell 测试脚本验证：
```powershell
.\test_web_search.ps1
```

或在 Web 界面：
1. 访问 http://localhost:8080/browse
2. 登录（admin / admin123）
3. 输入搜索关键词
4. 按回车或点击"全文搜索"

## 提交信息
- **Commit**: 0416415
- **Branch**: main
- **Message**: "feat: Add full-text content search to web UI"

## 相关文件
- `server/web_server.cpp` - Web 服务器和 HTML 模板
- `server/server.cpp` - 搜索广播和结果聚合
- `client/file_indexer.cpp` - 全文搜索实现（OR 混合策略）
- `test_web_search.ps1` - API 测试脚本

## 已知限制
1. FTS5 对某些中文词（如"雁塔"）可能无法分词匹配
   - 解决方案：LIKE 降级确保搜索可用
2. 搜索超时设置为 3 秒
   - 可在 `handleContentSearch()` 中调整
3. 全文搜索结果不显示文件大小（humanSize）
   - 后端 API 未返回此字段，可后续添加

## 未来优化
1. 添加搜索历史记录
2. 高亮显示匹配的文本片段
3. 支持高级搜索选项（日期范围、文件类型等）
4. 添加搜索结果排序选项
5. 优化 jieba 分词词典
