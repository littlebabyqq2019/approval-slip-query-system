# Simple FTS5 扩展集成说明

## 概述

为了支持中文全文检索，我们集成了 [Simple](https://github.com/wangfenjin/simple) FTS5 扩展。Simple 使用 jieba 中文分词器，可以正确处理中文词组搜索。

## 问题背景

### 为什么需要 Simple？

SQLite FTS5 默认的 `unicode61` 分词器按空格和标点符号分词，对中文支持不佳：

**问题示例**：
- 搜索 "雁塔" 时，`unicode61` 分词成 '雁' AND '塔'
- 数据库中存储的是独立字符，无法匹配多字符词组
- 同样，英文词 'cosmos' 也会被拆成 'c', 'o', 's', 'm', 'o', 's'

**结果**：即使数据已正确索引，搜索也返回 0 结果。

### Simple 如何解决？

Simple 使用 jieba 分词器，可以：
- 正确识别中文词组（如 "雁塔"、"西安"）
- 支持自定义词典
- 与 FTS5 完全兼容

## 架构设计

### 自动降级机制

客户端代码设计了智能检测机制：

```cpp
// 1. 尝试加载 Simple 扩展
loadSimpleExtension();

// 2. 检测可用的最佳分词器
QString tokenizer = detectBestTokenizer();

// 3. 创建 FTS5 表时使用检测到的分词器
CREATE VIRTUAL TABLE files_fts USING fts5(
    ...,
    tokenize='simple'  // 或 'unicode61' 作为备选
)
```

**好处**：
- ✅ Simple 可用时自动启用（最佳性能）
- ✅ Simple 不可用时降级到 unicode61（基本功能）
- ✅ 不会因为缺少 Simple 而导致功能完全失效

### 文件结构

```
CrossNetShareClient.exe
simple.dll              ← Simple 扩展库
dict/                   ← jieba 词典文件
  ├── jieba.dict.utf8
  ├── hmm_model.utf8
  └── user.dict.utf8
plugins/
  └── sqldrivers/
      └── qsqlite.dll
```

## 构建流程

### GitHub Actions 自动构建

在 `.github/workflows/build.yml` 中添加了 Simple 构建步骤：

1. **克隆 Simple 仓库**
   ```powershell
   git clone --depth 1 https://github.com/wangfenjin/simple.git
   ```

2. **编译 Simple 扩展**
   ```powershell
   cmake .. -G "Visual Studio 17 2022" -A x64
   cmake --build . --config Release
   ```

3. **复制到输出目录**
   ```powershell
   Copy-Item simple.dll build/bin/Release/
   Copy-Item dict/* build/bin/Release/dict/
   ```

### 本地构建

使用提供的 `build_simple.bat` 脚本：

```batch
build_simple.bat
```

脚本会：
1. 克隆 Simple 仓库到 `third_party/simple`
2. 配置 CMake 构建
3. 编译生成 `simple.dll`
4. 提示复制位置

## 使用说明

### 客户端日志

启动客户端时，检查日志输出：

**成功加载 Simple**：
```
[FileIndexer] Looking for Simple extension: C:/path/to/simple.dll
[FileIndexer] Found Simple extension, attempting to load...
[FileIndexer] Simple extension loaded successfully!
[FileIndexer] Chinese full-text search enabled with jieba tokenizer
[FileIndexer] Using tokenizer: simple
```

**降级到 unicode61**：
```
[FileIndexer] Simple extension not found, will use unicode61 tokenizer
[FileIndexer] Note: Chinese search may not work optimally
[FileIndexer] Using tokenizer: unicode61 remove_diacritics 2
```

### 搜索行为

#### 使用 Simple 分词器

```
搜索: "雁塔"
分词: ["雁塔"]        ← 识别为一个词
结果: ✅ 找到包含"雁塔"的文档
```

#### 使用 unicode61 分词器

```
搜索: "雁塔"
分词: ["雁", "塔"]    ← 拆成两个字
查询: 雁 AND 塔
结果: ❌ 可能找不到（除非恰好两字相邻）
```

## 故障排查

### Simple 扩展加载失败

**可能原因**：
1. `simple.dll` 不存在
2. 依赖的 DLL 缺失（如 MSVC Runtime）
3. SQLite 版本不兼容

**解决方案**：
1. 检查文件是否存在：
   ```batch
   dir /b CrossNetShareClient\simple.dll
   ```

2. 使用 Dependencies Walker 检查依赖：
   ```
   https://github.com/lucasg/Dependencies
   ```

3. 查看客户端日志中的错误信息

### 中文搜索仍然不工作

**检查清单**：
1. ✅ 确认 Simple 扩展已加载
2. ✅ 确认使用的是 'simple' tokenizer（查看日志）
3. ✅ 重建索引（旧索引可能使用了 unicode61）
4. ✅ 检查词典文件是否存在

**重建索引**：
```
设置 → 索引设置 → 清除索引 → 重建索引
```

## 性能影响

### 索引速度

| 分词器 | 索引速度 | 备注 |
|--------|----------|------|
| unicode61 | 快 | 简单分割，无需分词 |
| simple (jieba) | 中等 | 需要中文分词，稍慢 |

**测试数据**：1000 个文件约需 2-5 分钟（取决于 CPU）

### 搜索速度

| 分词器 | 搜索速度 | 准确性 |
|--------|----------|--------|
| unicode61 | 快 | 中文差 |
| simple | 快 | 中文优秀 |

**结论**：Simple 对搜索速度影响极小，但准确性大幅提升。

## 版本信息

- **Simple 版本**：latest (main branch)
- **Jieba 版本**：内置于 Simple
- **SQLite 版本**：Qt 5.15.2 自带
- **FTS5 版本**：SQLite 内置

## 参考资料

- Simple 项目：https://github.com/wangfenjin/simple
- SQLite FTS5：https://www.sqlite.org/fts5.html
- Jieba 分词：https://github.com/fxsjy/jieba

## 更新日志

**v1.4.0**
- ✅ 集成 Simple FTS5 扩展
- ✅ 自动降级到 unicode61 机制
- ✅ GitHub Actions 自动构建 Simple
- ✅ 本地构建脚本 `build_simple.bat`

---

**最后更新**：2026-08-31  
**维护者**：CrossNetShare Team
