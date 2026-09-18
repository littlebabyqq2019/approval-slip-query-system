# 客户端崩溃问题修复 - v2.2.2

## 问题症状

**用户报告**:
- 客户端启动几秒后自动退出，无法正常使用
- 索引完成后程序直接崩溃
- 崩溃发生太快，无法复制日志内容

## 问题诊断过程

### 第一阶段：添加异常捕获（未解决）

**尝试修复**:
1. 为 `rebuildIndex()` 和 `updateIndex()` 添加 try-catch
2. 为 `search()` 函数添加异常保护
3. 为 `getStats()` 和 `indexingFinished` 槽函数添加 try-catch

**结果**: 无效，程序仍然崩溃

**提交**: 2a80f6d, fb35be7, f98b914

### 第二阶段：添加日志文件功能（关键突破）

**修改**:
- 在 `MainWindow::appendLog()` 中添加日志文件输出
- 日志保存到 `client_log.txt`，追加模式
- 立即刷新 `flush()` 确保崩溃前写入

**结果**: 成功获取崩溃时的日志

**提交**: 961c921

### 第三阶段：分析日志找到根本原因

**关键日志片段**:
```
[2026-09-03 15:34:53] [FileIndexer] Incremental update completed: 14 files indexed, 6353 skipped, 0 removed
（日志在这里结束，没有后续内容）
```

**关键发现**:
1. 日志在 "Incremental update completed" 后立即结束
2. **没有** "Content indexing finished" 日志
3. **没有** "Indexed X files, total size: X MB" 日志
4. 这意味着 `emit indexingFinished()` 发送了，但槽函数没有执行

### 第四阶段：定位线程同步问题

**代码分析**:

```cpp
// file_indexer.cpp - updateIndex() 在后台线程中执行
QtConcurrent::run([this]() {
    // ... 索引处理 ...
    emit logMessage("[FileIndexer] Incremental update completed: ...");
    emit indexingFinished();  // ← 从后台线程 emit
});

// main_window.cpp - 槽函数在主线程
connect(indexer_, &FileIndexer::indexingFinished, [this]() {
    onLogMessage("Content indexing finished");  // ← 从未执行
    // ...
});
```

**问题**:
- `QtConcurrent::run()` 创建的后台线程 emit 信号
- 默认连接（AutoConnection）可能在线程结束时出现问题
- 信号发送后线程立即结束，槽函数可能访问已释放的内存

## 最终解决方案

### 修复代码

在 `MainWindow::initializeIndexer()` 中为所有后台线程信号添加 `Qt::QueuedConnection`:

```cpp
// 使用 Qt::QueuedConnection 确保从后台线程发送的日志消息能安全传递到主线程
connect(indexer_, &FileIndexer::logMessage, this, &MainWindow::onLogMessage, 
        Qt::QueuedConnection);

connect(indexer_, &FileIndexer::indexingStarted, this, [this]() {
    onLogMessage("Content indexing started...");
}, Qt::QueuedConnection);

connect(indexer_, &FileIndexer::indexingFinished, this, [this]() {
    try {
        onLogMessage("Content indexing finished");
        IndexStats stats = indexer_->getStats();
        onLogMessage(QString("Indexed %1 files, total size: %2 MB")
            .arg(stats.totalFiles)
            .arg(stats.indexSizeMB));
    } catch (const std::exception& e) {
        onLogMessage(QString("Error in indexingFinished handler: %1").arg(e.what()));
    } catch (...) {
        onLogMessage("Error in indexingFinished handler: Unknown exception");
    }
}, Qt::QueuedConnection);

connect(indexer_, &FileIndexer::indexingError, this, [this](const QString& error) {
    onLogMessage("Indexing error: " + error);
}, Qt::QueuedConnection);
```

### 为什么这样修复有效

**Qt 信号槽连接类型**:

1. **Qt::DirectConnection**: 
   - 槽函数在信号发送者的线程中直接执行
   - 同步调用
   - 如果跨线程使用可能导致线程安全问题

2. **Qt::QueuedConnection**: 
   - 信号被放入接收者线程的事件队列
   - 异步调用
   - 槽函数在接收者线程的事件循环中执行
   - **线程安全**

3. **Qt::AutoConnection** (默认): 
   - 如果信号和槽在同一线程：使用 DirectConnection
   - 如果信号和槽在不同线程：使用 QueuedConnection
   - 但在某些场景（如 `QtConcurrent::run()` 创建的临时线程）可能判断失误

**问题根源**:
- `QtConcurrent::run()` 创建的是临时线程，执行完立即销毁
- 后台线程 emit 信号后立即结束
- 如果使用 DirectConnection，主线程可能在访问已释放的线程资源
- 如果 AutoConnection 判断错误，也可能出现问题

**解决方法**:
- 显式使用 `Qt::QueuedConnection`
- 信号通过事件队列传递，与线程生命周期解耦
- 槽函数在主线程事件循环中安全执行
- 即使后台线程已结束，信号仍能正确传递和处理

## 技术教训

### 1. 跨线程信号必须显式指定连接类型

虽然 `Qt::AutoConnection` 理论上能自动处理，但在以下场景应显式使用 `Qt::QueuedConnection`:
- `QtConcurrent::run()` 创建的临时线程
- 自定义 QThread 发送的信号
- 任何可能在后台线程中 emit 的信号

### 2. try-catch 无法捕获所有崩溃

C++ 异常机制只能捕获 throw 出来的异常，无法捕获：
- 访问违规（access violation）
- 段错误（segmentation fault）
- 线程同步问题导致的崩溃
- 未定义行为（undefined behavior）

### 3. 日志文件是诊断崩溃的关键工具

当程序崩溃太快无法查看日志时：
- 实现日志文件功能（追加模式）
- 每条日志立即 flush() 到文件
- 崩溃后可以查看完整日志
- 能精确定位崩溃位置

### 4. 分析日志时注意"没有出现的日志"

```
✅ 出现的日志: "Incremental update completed"
❌ 没有出现的日志: "Content indexing finished"
```

"没有出现的日志"往往更重要，它告诉我们程序在哪里中断了。

## 验证清单

修复后应该看到完整的日志：
```
[时间] Content indexing started...
[时间] [FileIndexer] Starting incremental index update...
[时间] [FileIndexer] Scanning files in shared directory...
[时间] [FileIndexer] Found X files to check
[时间] [FileIndexer] X files already indexed (skipped), Y files need indexing
[时间] [FileIndexer] Indexing Y files...
... (索引进度日志) ...
[时间] [FileIndexer] Incremental update completed: Y files indexed, X skipped, 0 removed
[时间] Content indexing finished                    ← 这行之前缺失
[时间] Indexed X files, total size: X MB            ← 这行之前缺失
```

## 相关文件

- `client/file_indexer.cpp`: 索引执行（后台线程）
- `client/ui/main_window.cpp`: 信号连接和日志记录（主线程）
- `CHANGELOG_v2.2.2.md`: 版本更新日志
- `client_log.txt`: 用户提供的崩溃日志

## 提交信息

- **Commit**: 0f55ee5
- **Version**: 2.2.2
- **Date**: 2026-09-03
- **Fix**: 使用 Qt::QueuedConnection 修复跨线程信号崩溃问题
