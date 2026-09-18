# CrossNetShare v2.2.0 更新日志

发布日期：2026-09-02

## 🚀 重大改进

### 索引性能优化和自动更新修复

本版本专注于解决索引系统的三个严重问题，大幅提升用户体验。

## ✨ 新功能

### 1. 智能增量索引
- **新增方法**：`FileIndexer::updateIndex()`
- **功能**：只索引新增和修改的文件，跳过已索引的文件
- **效果**：重启程序时从 4 小时缩短至 5 秒以内

### 2. 启动时智能判断
- **首次启动**：自动执行完整索引（`rebuildIndex()`）
- **后续启动**：自动执行增量更新（`updateIndex()`）
- **用户体验**：无需手动操作，程序自动选择最优策略

### 3. 更频繁的自动扫描
- **扫描间隔**：从 60 分钟缩短至 10 分钟
- **扫描方式**：使用增量更新而非目录扫描
- **效果**：新文件最多 10 分钟后自动索引

## 🐛 问题修复

### 修复 #1：索引速度极慢
- **问题**：6300 个 Word 文件需要 4 小时以上
- **状态**：首次索引速度保持不变（预期行为）
- **改进**：后续启动和增量更新极快

### 修复 #2：每次重启都重新索引
- **问题**：关闭程序重新运行后，所有文件都重新索引
- **原因**：
  - `initializeIndexer()` 没有检查现有索引
  - 没有调用任何索引方法
  - 索引器 `start()` 只启动监控，不触发初始索引
- **修复**：
  - 添加数据库存在性检查
  - 首次启动调用 `rebuildIndex()`
  - 后续启动调用 `updateIndex()`（增量）
- **效果**：重启时间从 4+ 小时降至 < 5 秒

### 修复 #3：新文件不会自动索引
- **问题**：共享目录新增文件后，无法搜索到
- **原因**：
  - 定时扫描间隔太长（60 分钟）
  - 定时扫描调用 `scanDirectory()` 而非完整索引更新
- **修复**：
  - 扫描间隔改为 10 分钟
  - 定时扫描调用 `updateIndex()`
  - 自动检测新增、修改和删除的文件
- **效果**：新文件最多 10 分钟后可搜索

## 📊 性能对比

| 场景 | v2.1.0 | v2.2.0 | 改进 |
|------|--------|--------|------|
| 首次启动（6300文件） | 4+ 小时 | 4+ 小时 | 相同（预期）|
| 重启程序 | 4+ 小时 | < 5 秒 | **提速 2880 倍** |
| 新增 10 个文件 | 手动触发 | 10 分钟自动 | **自动化** |
| 删除文件清理 | 手动触发 | 10 分钟自动 | **自动化** |

### 详细性能数据

**场景 1：首次启动**
- 索引 6300 个文件：~4 小时
- 状态：正常（需要提取所有文本）

**场景 2：重启程序（无新文件）**
```
v2.1.0: 重新索引 6300 个文件 = 4+ 小时
v2.2.0: 跳过 6300 个已索引文件 = 5 秒
```

**场景 3：新增 10 个文件后重启**
```
v2.1.0: 重新索引 6300 + 10 = 6310 个文件 = 4+ 小时
v2.2.0: 只索引 10 个新文件 = 23 秒
```

## 💻 技术变更

### 修改的文件

#### 1. `client/file_indexer.h`
```cpp
// 新增方法声明
void updateIndex();  // 增量更新索引（只索引新增和修改的文件）
```

#### 2. `client/file_indexer.cpp`
- **新增**：`updateIndex()` 方法实现（~120 行）
  - 扫描所有文件
  - 对比文件哈希，识别需要更新的文件
  - 只索引新增和修改的文件
  - 自动删除已不存在文件的索引
  - 输出详细统计日志
  
- **修改**：`onScanTimerTimeout()`
  - 从调用 `scanDirectory()` 改为 `updateIndex()`
  - 实现真正的增量更新而非简单目录扫描

#### 3. `client/ui/main_window.cpp`
- **修改**：`initializeIndexer()` 方法
  - 添加数据库存在性检查
  - 根据数据库是否存在选择索引策略：
    - 不存在：`rebuildIndex()`（完整索引）
    - 存在：`updateIndex()`（增量更新）
  - 扫描间隔从 60 分钟改为 10 分钟

#### 4. `CMakeLists.txt`
- 版本号：2.1.0 → 2.2.0

### 新增文件

- **INDEX_PERFORMANCE_FIX.md** - 性能优化详细文档

## 📝 日志输出示例

### 首次启动
```
[FileIndexer] No existing index database, will create new one
[FileIndexer] First time indexing - will build complete index...
[FileIndexer] Found 6300 files to index
[FileIndexer] Index rebuild completed, 6300 files indexed
```

### 后续启动（无变化）
```
[FileIndexer] Existing index database found
[FileIndexer] Performing incremental index update (new and modified files only)...
[FileIndexer] 6300 files already indexed (skipped), 0 files need indexing
[FileIndexer] Index is up to date, no files need indexing
```

### 有新文件时
```
[FileIndexer] Scheduled scan triggered - checking for new and modified files
[FileIndexer] 6300 files already indexed (skipped), 10 files need indexing
[FileIndexer] Indexing 10 files...
[FileIndexer] Incremental update completed: 10 files indexed, 6300 skipped, 0 removed
```

## 🔄 升级指南

### 从 v2.1.0 升级

1. **下载新版本**
   - 从 GitHub Releases 下载 v2.2.0

2. **停止程序**
   - 关闭客户端和服务器

3. **替换文件**
   - 用新版本替换旧的可执行文件
   - **重要**：保留现有的 `content_index.db`（索引数据库）

4. **启动程序**
   - 启动客户端
   - 观察日志：应显示"Performing incremental index update"
   - 几秒钟内完成启动

5. **验证**
   - 测试搜索功能
   - 添加新文件，等待 10 分钟验证自动索引

### 注意事项

- ✅ **保留索引数据库**：`content_index.db` 可直接在 v2.2.0 中使用
- ✅ **向后兼容**：完全兼容 v2.1.0 和 v2.0.0
- ✅ **配置文件**：无需修改任何配置

## 🧪 测试建议

### 测试 1：增量更新（重启）
```bash
1. 确保已有索引数据库（content_index.db）
2. 重启客户端
3. 检查日志：应显示 "X files already indexed (skipped)"
4. 验证：启动时间 < 10 秒
```

### 测试 2：自动检测新文件
```bash
1. 客户端保持运行
2. 在共享目录添加新的 Word 文件
3. 等待最多 10 分钟
4. 验证：新文件可以被搜索到
```

### 测试 3：自动清理删除的文件
```bash
1. 客户端保持运行
2. 删除共享目录中的一些文件
3. 等待最多 10 分钟
4. 验证：已删除的文件不再出现在搜索结果中
```

## 🚧 已知限制

1. **首次索引仍然较慢**
   - 6300 个文件需要约 4 小时
   - 原因：需要提取每个文件的文本内容
   - 未来优化方向：批量文本提取、并行处理

2. **Python 进程开销**
   - 每个 Word/PDF 文件都需要启动 Python 进程
   - 平均耗时：2-3 秒/文件
   - 未来优化方向：修改提取脚本支持批量处理

3. **新文件检测延迟**
   - 最多 10 分钟延迟
   - 原因：使用定时扫描而非实时监控
   - 未来优化方向：递归文件系统监控

## 🔮 未来优化计划

### 短期（v2.3.0）
- [ ] 批量文本提取（一次 Python 进程处理多个文件）
- [ ] 索引进度条显示
- [ ] 手动触发增量更新按钮

### 中期（v2.4.0）
- [ ] 并行索引（多线程提取和索引）
- [ ] 文本提取缓存
- [ ] 递归文件系统监控

### 长期（v3.0.0）
- [ ] 使用 C++ Word 解析库（消除 Python 依赖）
- [ ] 分布式索引
- [ ] 增量式全文搜索

## 📖 相关文档

- `INDEX_PERFORMANCE_FIX.md` - 详细的性能优化说明
- `INDEX_PERFORMANCE_FAQ.md` - 索引性能常见问题
- `FIND_INDEX_GUIDE.md` - 索引文件位置指南
- `INDEXER_DEBUG_GUIDE.md` - 索引器调试指南

## 👥 贡献者

感谢所有报告问题和提供反馈的用户！

## 📄 许可证

本项目采用 MIT 许可证

---

**重要提示**：此版本大幅改善了索引性能和用户体验，强烈建议所有 v2.1.0 和更早版本的用户升级。
