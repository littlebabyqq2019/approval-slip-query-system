# 搜索问题分析

## 问题描述

### 问题 1：Web 页面无法全文检索
- 需要确认 Web API 是否正常工作

### 问题 2：客户端搜索结果不完整

**测试数据**：
- `2026-文件批办单#6600-(B-26-2073).doc` - 包含"西安市雁塔区卫生健康局文件批办"、字母B
- `投诉批办单#C-26-3113.docx` - 包含"西安市雁塔区卫生健康局文件批办"、字母C
- `投诉批办单#C-26-2.pdf` - 包含"西安市雁塔区卫生健康局文件批办"、字母C
- `提示词.txt` - 包含"西安市雁塔区卫生健康局"
- `小程序.txt` - 包含许多英语字母
- `牛二+搭建思维用 - 题目 (1).pdf` - 包含 cos
- `1.txt` - 包含"cos 大家 你好 中国 国家 西安市雁塔区卫生健康局"

**错误案例**：
搜索 `"雁塔 or 中国"` 时：
- ✅ 找到：1.txt, 新建 文本文档 (2).txt
- ❌ 未找到：2026-文件批办单#6600-(B-26-2073).doc, 投诉批办单#C-26-3113.docx, 投诉批办单#C-26-2.pdf

## 根本原因分析

### FTS5 + jieba 分词问题

从日志分析：

1. **单独搜索"雁塔"**：FTS5 返回 0 结果，LIKE 降级找到 6 个文件
2. **单独搜索"中国"**：（日志中没有，但从 OR 结果推测）FTS5 能找到 2 个文件
3. **布尔搜索"雁塔 or 中国"**：FTS5 只返回 2 个文件（只匹配了"中国"）

**结论**：jieba 对"雁塔"的分词不符合预期，导致 FTS5 无法匹配。

### jieba 分词行为推测

jieba 可能将"雁塔"分解为：
- "雁" + "塔"（两个单字）
- 或者识别为"大雁塔"的一部分

因此搜索"雁塔"时：
- FTS5 MATCH "雁塔" → 找不到（因为索引中是"雁"和"塔"）
- LIKE "%雁塔%" → 能找到（字符串包含匹配）

## 解决方案

### 方案 1：修复布尔搜索的 LIKE 降级（推荐）

当前问题：布尔搜索 `"雁塔 or 中国"` 的 LIKE 降级逻辑有问题。

**当前行为**：
1. FTS5: "雁塔 OR 中国" → 只找到包含"中国"的 2 个文件
2. LIKE 降级：没有正确处理 OR 的每个词

**修复方案**：
需要修改 LIKE 降级逻辑，对 OR 查询的每个词都进行 LIKE 匹配。

### 方案 2：改进 FTS5 查询策略

对于无法分词的中文词组，尝试多种查询策略：
1. 先尝试完整词组：MATCH "雁塔"
2. 如果失败，尝试拆分：MATCH "雁 塔"
3. 如果仍失败，降级到 LIKE

### 方案 3：使用 phrase 查询

FTS5 支持短语查询：`MATCH '"雁塔"'`（注意双引号）
这会精确匹配连续的词组。

## 推荐修复步骤

### 立即修复：布尔搜索 LIKE 降级

问题在于当前的 LIKE 降级可能没有被正确触发，或者 OR 逻辑有问题。

查看代码：
```cpp
// OR 条件
if (!orTerms.isEmpty()) {
    QStringList orConditions;
    for (const QString& term : orTerms) {
        orConditions << "(fts.content LIKE ? OR fts.file_name LIKE ?)";
        likeBindValues << ("%" + term + "%") << ("%" + term + "%");
    }
    likeConditions << "(" + orConditions.join(" OR ") + ")";
}
```

这个逻辑看起来是正确的，但从日志看：
```
[FileIndexer] Boolean search completed: 2 results  ← FTS5 找到 2 个
[FileIndexer] FTS5 boolean search returned no results, trying LIKE fallback...  ← 这行不应该出现！
[FileIndexer] LIKE boolean fallback completed: 0 results  ← LIKE 也没找到
```

**问题**：`results.isEmpty()` 判断错误！FTS5 已经返回了 2 个结果，所以不会触发 LIKE 降级。

### 修复方案：混合查询

对于布尔 OR 查询，应该：
1. 先用 FTS5 查询
2. 然后用 LIKE 查询所有 OR 词
3. 合并去重结果

这样可以确保：
- FTS5 能匹配的词（如"中国"）用 FTS5
- FTS5 不能匹配的词（如"雁塔"）用 LIKE
- 最终返回并集

## Web 搜索问题

需要确认：
1. Web API 端点是否正确实现
2. 客户端是否正确响应搜索请求
3. 结果是否正确聚合

测试方法：
```bash
curl -X POST http://localhost:8080/api/content-search \
  -H "Content-Type: application/json" \
  -H "Cookie: CNS_SESSION=<token>" \
  -d '{"query": "雁塔"}'
```

## 下一步

1. ✅ 分析问题 - 已完成
2. ⏳ 修复布尔搜索 OR 的 LIKE 降级逻辑
3. ⏳ 测试 Web 搜索 API
4. ⏳ 验证修复效果
