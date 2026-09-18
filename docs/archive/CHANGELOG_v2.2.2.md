# CrossNetShare v2.2.2 更新日志

**发布日期**: 2026-09-03

## 🐛 重要 Bug 修复

### 修复客户端启动后立即崩溃的问题

**问题描述**:
- 客户端在启动几秒后自动退出
- 日志显示索引完成后程序立即崩溃
- 日志文件显示 "Incremental update completed" 后没有进入 `indexingFinished` 槽函数

**问题根源**:
- `FileIndexer::updateIndex()` 和 `rebuildIndex()` 使用 `QtConcurrent::run()` 在后台线程中执行
- 这些线程中 emit 的信号 (`indexingFinished`, `indexingStarted`, `indexingError`, `logMessage`) 使用默认连接方式
- 默认情况下，信号-槽连接是 `Qt::AutoConnection`，但跨线程调用时可能导致线程安全问题
- 当后台线程 emit 信号后立即结束，而槽函数尚未执行或正在执行时，可能访问已释放的内存或出现竞态条件

**修复方案**:
- 在 `MainWindow::initializeIndexer()` 中为所有可能从后台线程触发的信号连接指定 `Qt::QueuedConnection`
- 这确保信号通过事件队列安全地从后台线程传递到主线程
- 受影响的信号：
  - `FileIndexer::indexingStarted`
  - `FileIndexer::indexingFinished`
  - `FileIndexer::indexingError`
  - `FileIndexer::logMessage`

**修改文件**:
- `client/ui/main_window.cpp`:
  - 所有 `indexer_` 的信号连接添加 `Qt::QueuedConnection` 参数
  - 确保信号处理在主线程的事件循环中安全执行

## 技术细节

### Qt 跨线程信号槽机制

Qt 提供三种信号-槽连接类型：
1. **Qt::DirectConnection**: 直接调用，信号和槽在同一线程（或发送者线程）
2. **Qt::QueuedConnection**: 通过事件队列，槽函数在接收者线程执行
3. **Qt::AutoConnection**: 自动选择（同线程用 Direct，跨线程用 Queued）

虽然 `Qt::AutoConnection` 理论上应该自动处理跨线程情况，但在某些场景下（如 `QtConcurrent::run()` 创建的临时线程）可能出现问题。显式使用 `Qt::QueuedConnection` 更加安全可靠。

### 为什么之前的 try-catch 没有捕获崩溃

前面尝试添加的 try-catch 无法捕获这类崩溃，因为：
1. 崩溃发生在信号传递机制层面，不是 C++ 异常
2. 可能是访问违规（access violation）或段错误（segfault），这些是操作系统级别的错误
3. 后台线程结束后，主线程访问了已失效的内存或对象

## 测试建议

1. 启动客户端，观察索引过程是否完整
2. 日志应该显示：
   - "Content indexing started..."
   - "[FileIndexer] Incremental update completed: ..."
   - "Content indexing finished"
   - "Indexed X files, total size: X MB"
3. 客户端应保持运行，不再崩溃

## 升级说明

直接替换 `CrossNetShareClient.exe` 即可，无需删除索引文件。

---

**版本**: v2.2.2
**上一版本**: v2.2.1
**提交**: (待生成)
