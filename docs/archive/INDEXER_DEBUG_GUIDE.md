# 索引器调试指南

## ✅ 最新更新 (提交 6511f94)

已添加详细的调试日志到索引器初始化代码，GitHub Actions 编译成功。

## 📥 下一步操作

### 1. 下载最新版本

从 GitHub Actions 下载最新编译的客户端：
- 访问：https://github.com/littlebabyqq2019/crossnet-share/actions
- 点击最上面的 "Add detailed debug logging to indexer initialization" 工作流
- 在 "Artifacts" 部分下载 `CrossNetShare-Client-Windows`

### 2. 替换并运行

1. 解压下载的文件
2. 替换现有的 `CrossNetShareClient.exe`
3. 启动客户端
4. 查看日志输出

### 3. 预期的日志输出

#### 成功初始化（理想情况）：
```
[时间] Configuration loaded successfully
[时间] Config values - Host: 127.0.0.1, Port: 8888, ClientID: B, SharePath: C:/Users/asusu/Desktop/1212
[时间] Connected to server
[时间] Registration: Registration successful
[时间] Attempting to initialize indexer with path: C:/Users/asusu/Desktop/1212
[时间] Database path: C:/Users/asusu/AppData/Local/CrossNetShareClient/content_index.db
[时间] App data path exists: yes
[时间] [FileIndexer] Initializing with shared path: C:/Users/asusu/Desktop/1212
[时间] [FileIndexer] Database path: C:/Users/asusu/AppData/Local/CrossNetShareClient/content_index.db
[时间] [FileIndexer] Checking for existing database connection...
[时间] [FileIndexer] Creating new database connection
[时间] [FileIndexer] Available SQL drivers: QSQLITE (或其他驱动)
[时间] [FileIndexer] Opening database: C:/Users/asusu/AppData/Local/CrossNetShareClient/content_index.db
[时间] [FileIndexer] Database opened successfully, creating tables...
[时间] [FileIndexer] Creating files table...
[时间] [FileIndexer] Files table created, creating FTS5 table...
[时间] [FileIndexer] FTS5 table created, creating config table...
[时间] [FileIndexer] Index database tables created successfully
[时间] [FileIndexer] Tables created successfully
[时间] [FileIndexer] Successfully initialized for path: C:/Users/asusu/Desktop/1212
[时间] Content indexer initialized and started
[时间] File system watcher started for: C:/Users/asusu/Desktop/1212
[时间] Scheduled scan started, interval: 60 minutes
[时间] FileIndexer started
```

#### 失败场景1：SQLite 驱动缺失
```
[时间] [FileIndexer] Available SQL drivers: 
[时间] [FileIndexer] Opening database: ...
[时间] [FileIndexer] Failed to open index database: Driver not loaded
```
**解决方案**：Qt 部署不完整，需要 `qsqlite.dll`

#### 失败场景2：FTS5 不支持
```
[时间] [FileIndexer] Creating files table...
[时间] [FileIndexer] Files table created, creating FTS5 table...
[时间] [FileIndexer] Failed to create FTS5 table: no such module: fts5
[时间] [FileIndexer] This may indicate FTS5 is not available in your SQLite build
[时间] [FileIndexer] Try checking SQLite version and FTS5 support
[时间] [FileIndexer] Failed to create tables
[时间] Failed to initialize content indexer - check database permissions and SQLite driver
```
**解决方案**：SQLite 未启用 FTS5，需要使用不同的 SQLite 版本或改用 FTS4

#### 失败场景3：权限问题
```
[时间] [FileIndexer] Opening database: C:/Users/asusu/AppData/Local/CrossNetShareClient/content_index.db
[时间] [FileIndexer] Failed to open index database: unable to open database file
[时间] [FileIndexer] Database path: C:/Users/asusu/AppData/Local/CrossNetShareClient/content_index.db
```
**解决方案**：目录权限问题或路径不存在

## 🔍 根据日志诊断问题

### 问题A：没有看到 [FileIndexer] 开头的日志
**原因**：客户端版本太旧，未包含调试日志  
**解决**：确保使用最新下载的版本（提交 6511f94 之后）

### 问题B：看到 "Available SQL drivers:" 后面是空的
**原因**：Qt SQLite 驱动未部署  
**解决**：
1. 检查 `plugins/sqldrivers/qsqlite.dll` 是否存在
2. 如果不存在，从 Qt 安装目录复制过来
3. 或重新下载 GitHub Actions 编译的完整包

### 问题C：看到 "no such module: fts5"
**原因**：SQLite 编译时未启用 FTS5 扩展  
**解决方案1（推荐）**：修改代码使用 FTS4
```cpp
// 在 file_indexer.cpp 的 createTables() 中
// 将 fts5 改为 fts4
"CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts4("
```

**解决方案2**：使用支持 FTS5 的 SQLite 版本
- 需要重新编译 Qt SQLite 插件，启用 FTS5

### 问题D：看到 "unable to open database file"
**原因**：数据库文件路径无法访问  
**解决**：
1. 检查 AppData 目录是否存在和可写
2. 手动创建目录：`C:\Users\asusu\AppData\Local\CrossNetShareClient\`
3. 测试权限：在该目录创建一个测试文件

## 📋 下载并运行后的检查清单

请完成以下步骤并提供日志：

- [ ] 已下载最新的 GitHub Actions 编译版本
- [ ] 已替换旧的 CrossNetShareClient.exe
- [ ] 已启动客户端并连接服务器
- [ ] 已查看完整日志输出
- [ ] 已复制完整日志（从启动到"Failed to initialize"的所有输出）

## 💡 提供日志时的注意事项

请提供**完整的**日志输出，包括：
1. 从启动到初始化失败的所有日志行
2. 特别关注所有 `[FileIndexer]` 开头的行
3. 注意 "Available SQL drivers:" 后面列出了什么
4. 看是否有 "Failed to..." 或错误相关的消息

## 🛠️ 如果仍然失败

如果使用最新版本后仍然显示 "Failed to initialize content indexer"，请提供：

1. **完整的日志输出**（从启动到失败）
2. **客户端版本确认**：在日志最开始应该能看到启动信息
3. **目录检查结果**：
   ```
   dir C:\Users\asusu\AppData\Local\CrossNetShareClient\
   ```
4. **驱动文件检查**：
   ```
   dir plugins\sqldrivers\
   ```

---

**当前状态**：✅ 代码已推送到 GitHub（提交 6511f94），编译成功  
**下一步**：用户下载最新版本并提供详细日志
