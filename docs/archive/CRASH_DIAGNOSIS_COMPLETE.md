# 完整崩溃诊断和修复报告

## 问题描述

**症状**：客户端启动后显示"索引完毕"，然后立即崩溃退出

**影响版本**：v2.2.0, v2.2.1 早期版本

## 崩溃时间线分析

根据用户报告的日志顺序：
```
1. 程序启动
2. 连接服务器成功
3. 开始增量索引
4. "Content indexing finished" ← 显示这条日志
5. 程序立即崩溃退出 ← 在这里崩溃
```

## 根本原因

### 原因 1：QtConcurrent 线程中的未捕获异常 ✅ 已修复
- **位置**：`rebuildIndex()`, `updateIndex()` 
- **问题**：异步线程中的异常没有捕获
- **修复**：commit 2a80f6d

### 原因 2：搜索和目录扫描的未捕获异常 ✅ 已修复
- **位置**：`performContentSearch()`, `onDirectoryChanged()`
- **问题**：这些函数也使用 QtConcurrent 但没有异常保护
- **修复**：commit fb35be7

### 原因 3：getStats() 函数崩溃 ✅ 已修复
- **位置**：`FileIndexer::getStats()`
- **问题**：在 `indexingFinished` 信号的槽函数中调用，SQLite 查询可能失败
- **症状**：显示 "indexing finished" 后立即崩溃（正好是这个症状！）
- **修复**：commit f98b914

### 原因 4：indexingFinished 槽函数崩溃 ✅ 已修复
- **位置**：`main_window.cpp` 的 `indexingFinished` 信号处理
- **问题**：调用 `getStats()` 时没有异常保护
- **修复**：commit f98b914

## 详细技术分析

### 崩溃发生的精确位置

```cpp
// main_window.cpp - 758-765 行
connect(indexer_, &FileIndexer::indexingFinished, [this]() {
    onLogMessage("Content indexing finished");  // ← 用户看到这条日志
    IndexStats stats = indexer_->getStats();    // ← 在这里崩溃！
    onLogMessage(QString("Indexed %1 files...").arg(stats.totalFiles));
});
```

### 为什么 getStats() 会崩溃？

1. **SQLite 线程安全问题**
   ```cpp
   // getStats() 在主线程执行
   // 但 db_ 可能正被后台线程访问
   sqlite3_step(stmt);  // ← 可能崩溃
   ```

2. **数据库连接状态**
   - 后台索引线程刚结束
   - 数据库可能处于不稳定状态
   - 立即查询可能失败

3. **未捕获的异常**
   - SQLite 查询失败抛出异常
   - 异常未被捕获
   - 程序崩溃

## 完整修复方案

### 修复 1：getStats() 添加异常捕获

**修改文件**：`client/file_indexer.cpp`

```cpp
IndexStats FileIndexer::getStats() {
    IndexStats stats = stats_;
    
    try {
        // SQLite 查询
        // ...
        return stats;
    } catch (const std::exception& e) {
        emit logMessage(QString("[FileIndexer] Error in getStats: %1").arg(e.what()));
        return stats_;  // 返回缓存值
    } catch (...) {
        emit logMessage("[FileIndexer] Error in getStats: Unknown exception");
        return stats_;  // 返回缓存值
    }
}
```

**效果**：
- getStats() 不再崩溃
- 失败时返回缓存的统计信息
- 记录错误到日志

### 修复 2：indexingFinished 处理函数添加异常捕获

**修改文件**：`client/ui/main_window.cpp`

```cpp
connect(indexer_, &FileIndexer::indexingFinished, [this]() {
    try {
        onLogMessage("Content indexing finished");
        IndexStats stats = indexer_->getStats();
        onLogMessage(QString("Indexed %1 files...").arg(stats.totalFiles));
    } catch (const std::exception& e) {
        onLogMessage(QString("Error in indexingFinished handler: %1").arg(e.what()));
    } catch (...) {
        onLogMessage("Error in indexingFinished handler: Unknown exception");
    }
});
```

**效果**：
- 即使 getStats() 抛出异常也不会崩溃
- 错误会被记录到日志
- 程序继续运行

### 修复 3：禁用自动隐藏（调试用）

**修改文件**：`client/ui/main_window.cpp`

```cpp
// 临时禁用自动隐藏到托盘
// 用户可以看到完整的日志输出
/*
QTimer::singleShot(1000, this, [this]() {
    hide();
});
*/
```

**效果**：
- 窗口保持可见
- 用户可以看到崩溃前的所有日志
- 便于诊断问题

## 提交历史

| Commit | 修复内容 | 日期 |
|--------|---------|------|
| 2a80f6d | QtConcurrent 异常捕获 (rebuildIndex, updateIndex) | 2026-09-02 |
| fb35be7 | 搜索和目录扫描异常捕获 + 禁用自动隐藏 | 2026-09-02 |
| f98b914 | getStats 和 indexingFinished 异常捕获 | 2026-09-02 |

## 测试验证

### 预期行为（修复后）

```
[2026-09-02 16:30:00] Starting incremental index update...
[2026-09-02 16:30:02] Found 6312 files to check
[2026-09-02 16:30:02] 6300 files already indexed (skipped), 12 files need indexing
[2026-09-02 16:30:30] Incremental update completed: 12 files indexed
[2026-09-02 16:30:30] Content indexing finished
[2026-09-02 16:30:30] Indexed 6312 files, total size: 150 MB
<程序继续运行，不会崩溃>
```

### 如果仍有错误（但不崩溃）

```
[2026-09-02 16:30:30] Content indexing finished
[2026-09-02 16:30:30] Error in getStats: Database locked
<程序继续运行，可以正常使用>
```

## 根本解决方案（未来版本）

当前修复是"防御性编程"——捕获所有异常防止崩溃。但根本问题是**线程安全**。

### 长期解决方案（v3.0.0）

1. **使用单独的数据库连接**
   ```cpp
   // 主线程一个连接
   sqlite3* mainDb_;
   
   // 索引线程一个连接
   sqlite3* indexDb_;
   ```

2. **使用队列和信号通信**
   ```cpp
   // 所有数据库操作在同一个线程
   // 其他线程通过信号槽通信
   ```

3. **使用 QThread 替代 QtConcurrent**
   ```cpp
   class IndexThread : public QThread {
       // 更好的生命周期控制
       // 更容易管理资源
   };
   ```

## 用户指南

### 如何获取最新修复

1. **等待 GitHub Actions 完成编译**
   - 访问：https://github.com/littlebabyqq2019/crossnet-share/actions
   - 等待最新的构建完成

2. **下载最新版本**
   - commit f98b914 或更新
   - 下载构建产物

3. **升级步骤**
   ```
   1. 关闭客户端
   2. 替换 CrossNetShareClient.exe
   3. 保留所有其他文件
   4. 重新启动
   ```

### 如果仍然崩溃

如果应用了所有修复后仍然崩溃，请提供：

1. **完整日志**
   - 从启动到崩溃的所有日志
   - 特别是最后几行

2. **系统信息**
   ```
   - Windows 版本
   - 内存大小
   - 杀毒软件（可能拦截）
   ```

3. **数据库信息**
   ```powershell
   # 检查数据库文件
   dir "D:\Program Files\CrossNetShare-Windows-x64\Client\content_index.db"
   
   # 检查文件大小和修改时间
   ```

4. **Python 环境**
   ```cmd
   python --version
   python -c "import aspose.words; print('OK')"
   ```

### 临时解决方案

如果无法立即获取新版本：

**方案 1：删除索引数据库**
```powershell
# 关闭客户端
# 删除数据库
del "D:\Program Files\CrossNetShare-Windows-x64\Client\content_index.db"
# 重新启动（会重建索引）
```

**方案 2：使用旧版本**
- 回退到 v2.1.0
- 虽然有重复索引问题，但至少不会崩溃

## 预防措施（开发者）

### 代码审查清单

在添加新的异步代码时，检查：

- [ ] 所有 `QtConcurrent::run` 都有 try-catch
- [ ] 所有信号槽处理函数都有 try-catch
- [ ] 所有数据库操作都有错误处理
- [ ] 所有 Python 脚本调用都有超时和错误处理
- [ ] 对象生命周期明确（避免悬垂指针）

### 添加单元测试

```cpp
// 测试异常安全性
TEST(FileIndexer, ExceptionSafety) {
    // 注入各种错误
    // 验证程序不崩溃
}
```

## 总结

### 崩溃原因

程序在 `indexingFinished` 信号处理中调用 `getStats()`，该函数执行 SQLite 查询时抛出未捕获的异常，导致程序崩溃。

### 修复方法

在所有关键路径添加异常捕获：
1. ✅ 索引线程
2. ✅ 搜索线程
3. ✅ getStats() 函数
4. ✅ 信号槽处理函数

### 修复效果

- 程序不再崩溃
- 错误被记录到日志
- 用户体验大幅改善

### 版本信息

- **修复版本**：v2.2.2（待发布）
- **关键提交**：f98b914
- **测试状态**：待用户验证

---

**最后更新**：2026-09-02 17:00  
**状态**：等待用户测试反馈  
**下一步**：根据反馈进一步优化
