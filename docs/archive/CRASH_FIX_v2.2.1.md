# 紧急修复：客户端启动后崩溃退出 (v2.2.1)

## 问题描述

**症状**：客户端打开后几秒钟自动退出

**影响版本**：v2.2.0

**严重性**：🔴 严重 - 程序无法正常使用

## 根本原因

### 问题 1：QtConcurrent 线程中的未捕获异常

**代码位置**：`client/file_indexer.cpp`
- `rebuildIndex()` 函数
- `updateIndex()` 函数

**问题**：
```cpp
QtConcurrent::run([this]() {
    // 如果这里抛出异常，会导致程序崩溃
    // 没有 try-catch 保护
});
```

**影响**：
- 索引过程中的任何异常都会导致程序崩溃
- 常见异常来源：
  - 文件访问错误
  - 内存不足
  - SQLite 操作失败
  - Python 脚本执行失败

### 可能触发崩溃的场景

1. **文件权限问题**
   - 无法读取某些文件
   - 无法访问数据库文件

2. **内存不足**
   - 扫描大量文件时内存耗尽
   - 文本提取时内存分配失败

3. **SQLite 错误**
   - 数据库被锁定
   - SQL 语句执行失败
   - 数据库损坏

4. **Python 脚本问题**
   - Python 未安装
   - Aspose.Words 库未安装
   - 脚本执行超时

## 修复方案

### 添加异常捕获

**修改后的代码结构**：
```cpp
QtConcurrent::run([this]() {
    try {
        // 所有索引操作
        // ...
    } catch (const std::exception& e) {
        emit logMessage(QString("[FileIndexer] FATAL ERROR: %1").arg(e.what()));
        isIndexing_ = false;
        emit indexingFinished();
    } catch (...) {
        emit logMessage("[FileIndexer] FATAL ERROR: Unknown exception");
        isIndexing_ = false;
        emit indexingFinished();
    }
});
```

**保护的内容**：
1. ✅ 文件扫描
2. ✅ 哈希计算
3. ✅ 数据库查询
4. ✅ 文件索引
5. ✅ 文本提取

**错误处理**：
- 记录详细的错误信息到日志
- 确保 `isIndexing_` 标志正确重置
- 触发 `indexingFinished()` 信号
- **不会崩溃**，只是停止索引

## 修复效果

| 场景 | v2.2.0 | v2.2.1 |
|------|--------|--------|
| 正常索引 | ✅ 工作 | ✅ 工作 |
| 文件权限错误 | ❌ 崩溃 | ✅ 记录错误，继续运行 |
| 内存不足 | ❌ 崩溃 | ✅ 记录错误，继续运行 |
| SQLite 错误 | ❌ 崩溃 | ✅ 记录错误，继续运行 |
| Python 错误 | ❌ 崩溃 | ✅ 记录错误，继续运行 |

## 测试建议

### 测试 1：正常启动
```
1. 启动客户端
2. 观察是否正常运行
3. 检查日志没有 "FATAL ERROR"
4. 程序不应退出
```

### 测试 2：索引错误恢复
```
1. 启动客户端
2. 如果索引过程中遇到错误
3. 应该在日志中看到错误信息
4. 程序继续运行，不会崩溃
```

### 测试 3：长时间运行
```
1. 启动客户端并让其索引
2. 等待 5-10 分钟
3. 程序应该稳定运行
4. 即使索引失败也不会崩溃
```

## 升级步骤

### 从 v2.2.0 升级到 v2.2.1

1. **停止程序**
   ```
   关闭客户端
   ```

2. **下载新版本**
   ```
   从 GitHub Actions 下载最新构建
   commit: 2a80f6d 或更新
   ```

3. **替换文件**
   ```
   用新的 CrossNetShareClient.exe 替换旧的
   保留所有其他文件（特别是 content_index.db）
   ```

4. **启动测试**
   ```
   启动客户端
   观察日志
   确认程序不会退出
   ```

## 相关提交

| Commit | 说明 | 日期 |
|--------|------|------|
| 2a80f6d | 添加异常处理防止崩溃 | 2026-09-02 |
| 143f8c8 | 修复重复 rebuildIndex 调用 | 2026-09-02 |
| 5f59b82 | 优化索引性能 (v2.2.0) | 2026-09-02 |

## 临时解决方案（如果无法立即升级）

如果你无法立即升级到 v2.2.1，可以尝试：

### 方案 1：删除索引数据库重新开始
```powershell
# 1. 关闭客户端
# 2. 删除索引数据库
del "D:\Program Files\CrossNetShare-Windows-x64\Client\content_index.db"
# 3. 重新启动客户端
```

### 方案 2：禁用自动索引
这需要修改代码或等待新版本支持配置选项。

### 方案 3：使用旧版本
回退到 v2.1.0 或 v2.0.0，虽然有重复索引问题但至少不会崩溃。

## 预防措施（未来版本）

### 1. 更健壮的线程管理
```cpp
// 使用 QThreadPool 而不是 QtConcurrent::run
// 或者使用 QThread 子类
```

### 2. 索引队列限制
```cpp
// 限制同时索引的文件数量
// 避免内存耗尽
```

### 3. 文件级别的异常处理
```cpp
// 每个文件索引失败不应影响其他文件
for (const QString& file : files) {
    try {
        indexFile(file);
    } catch (...) {
        // 只记录错误，继续下一个文件
    }
}
```

### 4. 超时机制
```cpp
// Python 脚本执行超时
// 单个文件索引超时
```

## 日志分析

### 正常日志（v2.2.1）
```
[FileIndexer] Starting incremental index update...
[FileIndexer] Scanning files in shared directory...
[FileIndexer] Found 6312 files to check
[FileIndexer] DEBUG: Check 'file1.doc', hash='d8e8601d', indexed=YES
...
[FileIndexer] 1400 files already indexed (skipped), 4912 files need indexing
[FileIndexer] Indexing 4912 files...
[FileIndexer] Progress: 10/4912 (0%)
...
```

### 错误日志（有异常但不崩溃）
```
[FileIndexer] Starting incremental index update...
[FileIndexer] Scanning files in shared directory...
[FileIndexer] FATAL ERROR in updateIndex: Access denied
[FileIndexer] Indexing finished (with errors)
```

### 崩溃日志（v2.2.0，程序会退出）
```
[FileIndexer] Starting incremental index update...
[FileIndexer] Scanning files in shared directory...
<程序突然退出，没有错误信息>
```

## 诊断工具

如果升级后仍有问题，请提供：

1. **完整的启动日志**
   - 从程序启动到崩溃/退出的所有日志

2. **系统信息**
   - 操作系统版本
   - 内存大小
   - 磁盘可用空间

3. **数据库状态**
   - content_index.db 文件大小
   - 是否可以访问

4. **Python 环境**
   ```cmd
   python --version
   python -c "import aspose.words; print('OK')"
   ```

## 版本信息

- **版本号**：v2.2.1
- **发布日期**：2026-09-02
- **基于版本**：v2.2.0
- **优先级**：🔴 紧急 - 修复崩溃问题
- **向后兼容**：✅ 完全兼容 v2.2.0

## 相关文档

- `INDEX_REINDEX_BUG_FIX.md` - 重复索引问题分析
- `INDEX_PERFORMANCE_FIX.md` - 性能优化说明
- `WORD_AUTO_RECONNECT_FIX.md` - 心跳机制修复

---

**总结**：v2.2.1 修复了 v2.2.0 中的严重崩溃问题。通过添加异常处理，程序现在可以优雅地处理索引错误，而不是崩溃退出。强烈建议所有 v2.2.0 用户立即升级。
