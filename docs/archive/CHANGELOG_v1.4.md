# CrossNetShare v1.4.0 更新日志

## 🚀 主要更新

### 中文全文检索支持

集成了 [Simple FTS5 扩展](https://github.com/wangfenjin/simple)，使用 jieba 分词器，完美支持中文搜索。

**问题修复**：
- ❌ **之前**：搜索"雁塔"返回 0 结果（unicode61 分词器不支持中文）
- ✅ **现在**：搜索"雁塔"正确返回包含该词的所有文件

### 智能降级机制

客户端会自动检测 Simple 扩展是否可用：
- ✅ Simple 可用 → 使用 jieba 分词（最佳中文支持）
- ⚠️ Simple 不可用 → 降级到 unicode61（基本功能）

**日志示例**：
```
[FileIndexer] Simple extension loaded successfully!
[FileIndexer] Chinese full-text search enabled with jieba tokenizer
[FileIndexer] Using tokenizer: simple
```

## 📝 技术细节

### 新增功能

1. **Simple 扩展集成**
   - 自动加载 `simple.dll`
   - 自动检测最佳分词器
   - 支持 jieba 词典

2. **构建流程改进**
   - GitHub Actions 自动编译 Simple
   - 本地构建脚本 `build_simple.bat`
   - 自动打包 simple.dll 和词典文件

3. **代码改进**
   - `FileIndexer::loadSimpleExtension()` - 加载扩展
   - `FileIndexer::detectBestTokenizer()` - 检测分词器
   - 详细的日志输出

### 文件变更

**新增文件**：
- `build_simple.bat` - 本地构建 Simple 扩展
- `SIMPLE_INTEGRATION.md` - Simple 集成文档

**修改文件**：
- `client/file_indexer.cpp` - 添加 Simple 支持
- `client/file_indexer.h` - 新增方法声明
- `.github/workflows/build.yml` - 自动构建 Simple
- `CMakeLists.txt` - 版本升级到 v1.4.0

## 🎯 使用说明

### 首次使用

1. 下载最新版本客户端
2. 启动后查看日志确认 Simple 已加载
3. 如果已有索引，建议**清除并重建索引**（使用新的分词器）

### 重建索引步骤

```
设置 → 索引设置 → 清除索引 → 重建索引
```

### 验证中文搜索

测试搜索关键词：
- "雁塔" - 应该能找到相关文档
- "西安" - 测试其他中文词
- 任意中文词组

## ⚙️ 系统要求

- **Windows 10/11** (x64)
- **Visual C++ Runtime** (如果 Simple 加载失败，安装 VC++ Redistributable)
- **磁盘空间**：额外 ~30 MB（simple.dll + 词典）

## 🐛 已知问题

- Simple 扩展在某些旧版 Windows 系统上可能需要额外的运行库
- 如果 Simple 加载失败，系统会自动降级到 unicode61，功能仍然可用

## 📚 相关文档

- [Simple 集成详细说明](SIMPLE_INTEGRATION.md)
- [全文检索功能状态](CONTENT_SEARCH_FINAL_STATUS.md)

## 🔄 从 v1.3.0 升级

**自动处理**：
- 客户端会自动检测并使用新的分词器
- 无需手动配置

**推荐操作**：
- 重建索引以获得最佳搜索效果
- 旧索引仍然可用，但使用旧的分词方式

---

**发布日期**：2026-08-31  
**版本号**：v1.4.0  
**分支**：main
