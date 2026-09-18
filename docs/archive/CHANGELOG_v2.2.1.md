# CrossNetShare v2.2.1 更新日志

发布日期：2026-09-02

## 🚨 紧急修复

### 修复程序崩溃问题

**问题**：v2.2.0 客户端启动后几秒钟自动退出

**根本原因**：
- `QtConcurrent::run()` 中的异步索引线程没有异常处理
- 任何未捕获的异常都会导致程序崩溃
- 常见触发因素：文件权限错误、SQLite 错误、Python 脚本失败

**修复内容**：
- ✅ 在 `rebuildIndex()` 中添加 try-catch 块
- ✅ 在 `updateIndex()` 中添加 try-catch 块
- ✅ 捕获所有异常并记录到日志
- ✅ 确保 `isIndexing_` 标志正确重置
- ✅ 程序不再因索引错误而崩溃

## 🐛 问题修复

### 修复 #1：QtConcurrent 线程崩溃

**问题描述**：
```cpp
// v2.2.0 - 危险代码
QtConcurrent::run([this]() {
    // 如果这里抛出异常，程序会崩溃
    indexFile(filePath);  
});
```

**修复后**：
```cpp
// v2.2.1 - 安全代码
QtConcurrent::run([this]() {
    try {
        indexFile(filePath);
    } catch (const std::exception& e) {
        emit logMessage(QString("ERROR: %1").arg(e.what()));
        // 程序继续运行
    }
});
```

### 修复 #2：错误日志

**新增功能**：
- 索引错误会记录到日志：`[FileIndexer] FATAL ERROR: ...`
- 用户可以看到具体的错误原因
- 便于诊断问题

## 📊 影响范围

| 场景 | v2.2.0 | v2.2.1 |
|------|--------|--------|
| 正常索引 | ✅ | ✅ |
| 文件权限错误 | ❌ 崩溃 | ✅ 记录错误 |
| 内存不足 | ❌ 崩溃 | ✅ 记录错误 |
| SQLite 错误 | ❌ 崩溃 | ✅ 记录错误 |
| Python 脚本失败 | ❌ 崩溃 | ✅ 记录错误 |

## 💻 技术变更

### 修改的文件

#### `client/file_indexer.cpp`

**1. rebuildIndex() 函数**
```cpp
// 添加了 try-catch 包裹整个异步操作
try {
    // 清空索引
    // 扫描文件
    // 索引文件
} catch (const std::exception& e) {
    emit logMessage(QString("[FileIndexer] FATAL ERROR in rebuildIndex: %1").arg(e.what()));
    isIndexing_ = false;
    emit indexingFinished();
} catch (...) {
    emit logMessage("[FileIndexer] FATAL ERROR in rebuildIndex: Unknown exception");
    isIndexing_ = false;
    emit indexingFinished();
}
```

**2. updateIndex() 函数**
```cpp
// 添加了 try-catch 包裹整个异步操作
try {
    // 扫描文件
    // 检查哈希
    // 增量索引
} catch (const std::exception& e) {
    emit logMessage(QString("[FileIndexer] FATAL ERROR in updateIndex: %1").arg(e.what()));
    isIndexing_ = false;
    emit indexingFinished();
} catch (...) {
    emit logMessage("[FileIndexer] FATAL ERROR in updateIndex: Unknown exception");
    isIndexing_ = false;
    emit indexingFinished();
}
```

#### `CMakeLists.txt`
- 版本号：2.2.0 → 2.2.1

### 新增文件
- **CRASH_FIX_v2.2.1.md** - 崩溃问题详细分析和修复说明

## 🔄 升级指南

### 从 v2.2.0 升级（强烈推荐）

1. **停止程序**
   - 关闭客户端

2. **替换文件**
   - 下载 v2.2.1 版本
   - 用新的 `CrossNetShareClient.exe` 替换旧的
   - **保留** `content_index.db` 和所有其他文件

3. **启动验证**
   - 启动客户端
   - 程序应该正常运行，不会退出
   - 检查日志没有 "FATAL ERROR"

### 从 v2.1.0 或更早版本升级

直接升级到 v2.2.1，获得所有 v2.2.0 的性能改进和崩溃修复。

## 📝 日志示例

### 正常运行（无错误）
```
[2026-09-02 16:00:00] Starting incremental index update...
[2026-09-02 16:00:01] Scanning files in shared directory...
[2026-09-02 16:00:02] Found 6312 files to check
[2026-09-02 16:00:02] 6300 files already indexed (skipped), 12 files need indexing
[2026-09-02 16:00:02] Indexing 12 files...
[2026-09-02 16:00:30] Incremental update completed: 12 files indexed, 6300 skipped, 0 removed
```

### 有错误但不崩溃
```
[2026-09-02 16:00:00] Starting incremental index update...
[2026-09-02 16:00:01] Scanning files in shared directory...
[2026-09-02 16:00:02] [FileIndexer] FATAL ERROR in updateIndex: Cannot access database
[2026-09-02 16:00:02] Indexing finished (with errors)
<程序继续运行，可以正常使用其他功能>
```

## 🧪 测试建议

### 测试 1：基本启动测试
```
1. 启动客户端
2. 等待 1 分钟
3. 验证：程序应该正常运行，不会退出
```

### 测试 2：索引功能测试
```
1. 启动客户端并等待索引完成
2. 或者手动触发：工具 → 索引设置 → 重建索引
3. 验证：即使有文件无法索引，程序也不会崩溃
```

### 测试 3：错误恢复测试
```
1. 临时修改共享目录权限（只读）
2. 启动客户端
3. 验证：程序记录权限错误但继续运行
4. 恢复权限后索引恢复正常
```

## 🔮 未来改进

虽然 v2.2.1 修复了崩溃问题，但仍有改进空间：

### 短期（v2.3.0）
- [ ] 单个文件索引失败不影响其他文件
- [ ] 更详细的错误分类和提示
- [ ] Python 脚本执行超时保护

### 中期（v2.4.0）
- [ ] 使用 QThread 替代 QtConcurrent 以更好控制
- [ ] 索引队列大小限制防止内存耗尽
- [ ] 文件索引重试机制

### 长期（v3.0.0）
- [ ] 使用 C++ Word 解析库消除 Python 依赖
- [ ] 沙箱化文本提取进程
- [ ] 分布式索引架构

## ⚠️ 已知限制

1. **索引错误不会重试**
   - 如果文件索引失败，不会自动重试
   - 需要手动重建索引或等待下次定时扫描

2. **错误日志可能不够详细**
   - 某些异常的错误信息可能较简略
   - 未来版本会改进

3. **部分索引可能不完整**
   - 如果某些文件无法访问，索引会跳过它们
   - 不会有明显的警告提示

## 📖 相关文档

- `CRASH_FIX_v2.2.1.md` - 崩溃问题详细分析
- `INDEX_PERFORMANCE_FIX.md` - v2.2.0 性能优化说明
- `INDEX_REINDEX_BUG_FIX.md` - 重复索引问题分析
- `CHANGELOG_v2.2.0.md` - v2.2.0 完整变更日志

## 🎉 总结

v2.2.1 是一个**紧急修复版本**，解决了 v2.2.0 中的严重崩溃问题。

**关键成就**：
- ✅ 修复了启动后自动退出的问题
- ✅ 添加了完整的异常处理
- ✅ 提供了详细的错误日志
- ✅ 程序稳定性大幅提升

**升级建议**：
- 🔴 **所有 v2.2.0 用户必须升级**（程序无法使用）
- 🟡 v2.1.0 和更早版本建议升级（获得性能改进）

---

**重要提示**：如果你使用 v2.2.0 遇到崩溃问题，请立即升级到 v2.2.1。这是一个关键的稳定性修复。

**版本**：v2.2.1  
**发布日期**：2026-09-02  
**提交哈希**：2a80f6d  
**优先级**：🔴 紧急  

CrossNetShare 开发团队
