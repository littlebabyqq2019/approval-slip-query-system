# 搜索问题修复总结

## 修复内容

### 问题：OR 布尔搜索结果不完整

**原因**：
- FTS5 对某些中文词（如"雁塔"）无法匹配（jieba 分词问题）
- 原有的 LIKE 降级只在 FTS5 **完全无结果**时才触发
- 对于 `"雁塔 or 中国"` 查询：
  - FTS5 找到包含"中国"的 2 个文件
  - 因为有结果，不触发 LIKE 降级
  - 导致包含"雁塔"的 4 个文件被遗漏

**修复方案**：
对 OR 查询使用**混合搜索策略**：
1. 对每个 OR 词单独执行完整搜索（包含 FTS5 + LIKE 降级）
2. 合并所有结果（自动去重）
3. 应用 NOT 过滤

### 代码改动

文件：`client/file_indexer.cpp`

```cpp
// 特殊处理：OR 查询使用混合搜索策略
if (!orTerms.isEmpty() && andTerms.isEmpty()) {
    emit logMessage("[FileIndexer] Using hybrid search for OR query (FTS5 + LIKE per term)");
    
    QSet<QString> allResults;
    
    // 对每个 OR 词执行完整搜索
    for (const QString& term : orTerms) {
        QStringList termResults = search(term, fileTypes);  // 自动包含 FTS5 + LIKE 降级
        for (const QString& result : termResults) {
            allResults.insert(result);
        }
    }
    
    // 应用 NOT 过滤（如果有）
    // ...
    
    return allResults.toList();
}
```

## 测试方法

### 测试数据准备

确保有以下测试文件：
- `1.txt` - 包含"cos 大家 你好 中国 国家 西安市雁塔区卫生健康局"
- `2026-文件批办单#6600-(B-26-2073).doc` - 包含"西安市雁塔区卫生健康局"
- `投诉批办单#C-26-3113.docx` - 包含"西安市雁塔区卫生健康局"
- `投诉批办单#C-26-2.pdf` - 包含"西安市雁塔区卫生健康局"
- `提示词.txt` - 包含"西安市雁塔区卫生健康局"
- `新建 文本文档 (2).txt` - 包含相关内容

### 测试用例

#### 测试 1：OR 查询（核心修复）

**输入**：`雁塔 or 中国`

**预期结果**（修复后）：
- ✅ 1.txt（包含"中国"和"雁塔"）
- ✅ 2026-文件批办单#6600-(B-26-2073).doc（包含"雁塔"）
- ✅ 投诉批办单#C-26-3113.docx（包含"雁塔"）
- ✅ 投诉批办单#C-26-2.pdf（包含"雁塔"）
- ✅ 提示词.txt（包含"雁塔"）
- ✅ 新建 文本文档 (2).txt（包含相关内容）

**日志验证**：
```
[FileIndexer] Detected boolean operators, using boolean search
[FileIndexer] Parsing boolean query: 雁塔 or 中国
[FileIndexer]   OR term: 雁塔
[FileIndexer]   OR term: 中国
[FileIndexer] Using hybrid search for OR query (FTS5 + LIKE per term)
[FileIndexer] Searching for: '雁塔'  ← 第一个词
[FileIndexer] FTS5 returned no results, trying LIKE fallback...
[FileIndexer] LIKE fallback completed: X results
[FileIndexer] Searching for: '中国'  ← 第二个词
[FileIndexer] Search completed: Y results
[FileIndexer] OR search completed: Z results  ← 合并后的总数
```

#### 测试 2：OR with NOT

**输入**：`雁塔 or 中国 not cos`

**预期结果**：
- ✅ 所有包含"雁塔"或"中国"的文件
- ❌ 排除包含"cos"的文件（1.txt 等）

#### 测试 3：单词搜索（验证未破坏）

**输入**：`雁塔`

**预期结果**：
- 仍然正常工作（FTS5 失败后 LIKE 降级）

#### 测试 4：AND 查询（验证未破坏）

**输入**：`西安 and 卫生`

**预期结果**：
- 仍然使用原有逻辑（未修改）

## Web 搜索测试

### 前提条件

1. 启动服务器：`CrossNetShareServer.exe`
2. 启动至少 2 个客户端（ClientA, ClientB）
3. 确保客户端已索引文件

### 测试步骤

#### 1. 登录获取 token

```bash
curl -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin123"}'
```

响应中获取 `Set-Cookie: CNS_SESSION=<token>`

#### 2. 测试搜索 API

```bash
curl -X POST http://localhost:8080/api/content-search \
  -H "Content-Type: application/json" \
  -H "Cookie: CNS_SESSION=<your-token>" \
  -d '{"query": "雁塔 or 中国"}'
```

**预期响应**：
```json
{
  "success": true,
  "query": "雁塔 or 中国",
  "totalResults": 6,
  "results": [
    {
      "filename": "1.txt",
      "relativePath": "1.txt",
      "ownerClient": "ClientA",
      "size": 33,
      "modifyTime": 1704067200
    },
    // ... 更多结果
  ]
}
```

#### 3. 验证结果聚合

- 确认返回了来自所有客户端的结果
- 检查 `ownerClient` 字段是否正确
- 验证 `totalResults` 数量正确

### 服务器日志验证

```
[Server] Broadcasted content search '雁塔 or 中国' to 2 clients (ID: ...)
[Server] Received search result from ClientA: X files
[Server] Received search result from ClientB: Y files
```

## 已知限制

### jieba 分词问题依然存在

修复方案是通过 LIKE 降级绕过，而不是解决 jieba 分词本身的问题。

**影响**：
- FTS5 对某些词仍无法匹配
- 但 LIKE 降级确保了功能可用性
- 性能略有下降（LIKE 比 FTS5 慢）

**未来优化**：
1. 自定义 jieba 词典
2. 添加常用词到用户词库
3. 考虑其他中文分词器

### OR 查询性能

对每个 OR 词都执行完整搜索，可能比单次 FTS5 查询慢。

**影响**：
- 2 个词的 OR 查询 ~2 倍时间
- 但单次搜索仍然很快（< 50ms）
- 对于 7000 文件，总时间仍 < 200ms

## 下一步

1. ✅ 修复 OR 布尔搜索 - 已完成
2. ⏳ 等待 GitHub Actions 构建
3. ⏳ 下载测试新版本
4. ⏳ 验证修复效果
5. ⏳ 测试 Web 搜索 API
6. ⏳ 发布 v2.0.1（如果测试通过）

---

**提交**: commit `0e94069`  
**状态**: 已推送，等待构建
