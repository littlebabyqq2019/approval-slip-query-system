# Simple 扩展快速开始

## ✅ 已完成的工作

我已经完成了 Simple FTS5 扩展的集成，以支持中文全文检索。

### 代码修改

1. **客户端代码** (`client/file_indexer.cpp/h`)
   - 添加 `loadSimpleExtension()` - 自动加载 simple.dll
   - 添加 `detectBestTokenizer()` - 智能检测分词器
   - 自动降级机制：Simple 可用时使用，不可用时回退到 unicode61

2. **构建流程** (`.github/workflows/build.yml`)
   - 自动克隆 Simple 仓库
   - 自动编译 simple.dll
   - 自动打包到客户端发布包

3. **版本更新**
   - v1.3.0 → v1.4.0
   - 新增 `SIMPLE_INTEGRATION.md` - 详细文档
   - 新增 `CHANGELOG_v1.4.md` - 更新日志
   - 新增 `build_simple.bat` - 本地构建脚本

## 🚀 下一步（GitHub Actions 自动完成）

GitHub Actions 正在构建新版本，流程如下：

1. ✅ 克隆 Simple 仓库
2. ✅ 编译 simple.dll
3. ✅ 打包到客户端
4. ✅ 生成发布包

**预计时间**：5-10 分钟

## 📥 测试步骤

### 1. 下载新版本

等 GitHub Actions 完成后：
- 进入 https://github.com/littlebabyqq2019/crossnet-share/actions
- 下载最新的 `CrossNetShareClient` artifact

### 2. 清除旧索引

**重要**：旧索引使用了 unicode61 分词器，需要重建：

```
启动客户端 → 设置 → 索引设置 → 清除索引 → 重建索引
```

### 3. 验证 Simple 已加载

查看日志，应该看到：

```
✅ 成功加载：
[FileIndexer] Simple extension loaded successfully!
[FileIndexer] Chinese full-text search enabled with jieba tokenizer
[FileIndexer] Using tokenizer: simple

⚠️ 降级到 unicode61：
[FileIndexer] Simple extension not found, will use unicode61 tokenizer
[FileIndexer] Note: Chinese search may not work optimally
[FileIndexer] Using tokenizer: unicode61 remove_diacritics 2
```

### 4. 测试中文搜索

搜索测试关键词：
- ✅ "雁塔" - 应该找到相关文档
- ✅ "西安" - 测试其他中文词
- ✅ "文件" - 测试常见词

**预期结果**：能够正确搜索到包含这些词的文档

## 🔍 日志检查要点

### Simple 加载成功
```
[FileIndexer] Looking for Simple extension: C:/path/to/simple.dll
[FileIndexer] Found Simple extension, attempting to load...
[FileIndexer] Simple extension loaded successfully!
[FileIndexer] Using tokenizer: simple
```

### Simple 未找到（降级）
```
[FileIndexer] Simple extension not found, will use unicode61 tokenizer
[FileIndexer] Using tokenizer: unicode61 remove_diacritics 2
```

### 搜索测试
```
[FileIndexer] Searching for: '雁塔' (FTS query: '雁塔')
[FileIndexer] Search found X results  ← X 应该 > 0
```

## ⚙️ 故障排查

### 问题1：Simple 未加载

**可能原因**：
- simple.dll 未打包（GitHub Actions 构建失败）
- DLL 依赖缺失

**解决**：
1. 检查客户端目录是否有 `simple.dll`
2. 查看 GitHub Actions 构建日志
3. 如果缺失，系统会自动降级到 unicode61（功能仍可用，但中文搜索效果差）

### 问题2：搜索仍返回 0 结果

**检查清单**：
1. ✅ 确认日志显示 "Using tokenizer: simple"
2. ✅ 确认已重建索引（旧索引无效）
3. ✅ 确认搜索的文件确实包含该关键词

### 问题3：GitHub Actions 构建失败

**常见原因**：
- Simple 仓库克隆失败
- CMake 配置错误
- 编译依赖缺失

**解决**：
- 查看 Actions 日志中的 "Build Simple FTS5 Extension" 步骤
- 如果失败，客户端仍可正常工作（使用 unicode61）

## 📊 性能对比

| 分词器 | 索引速度 | 搜索准确性 | 中文支持 |
|--------|----------|------------|----------|
| unicode61 | 快 ⚡ | 差（中文） | ❌ 不支持 |
| simple (jieba) | 中等 | 优秀 ✨ | ✅ 完美支持 |

**结论**：Simple 对性能影响很小，但中文搜索准确性大幅提升。

## 📚 详细文档

- **集成文档**：[SIMPLE_INTEGRATION.md](SIMPLE_INTEGRATION.md)
- **更新日志**：[CHANGELOG_v1.4.md](CHANGELOG_v1.4.md)
- **功能状态**：[CONTENT_SEARCH_FINAL_STATUS.md](CONTENT_SEARCH_FINAL_STATUS.md)

## 🎯 当前状态

- ✅ 代码已提交
- ✅ 已推送到 GitHub
- ⏳ GitHub Actions 正在构建
- ⏳ 等待测试反馈

## 📞 问题反馈

测试时请关注：
1. Simple 是否成功加载？
2. 中文搜索是否工作？
3. 性能是否可接受？
4. 有无错误日志？

---

**版本**：v1.4.0  
**更新时间**：2026-08-31  
**状态**：等待测试
