# CrossNetShare v1.5.0 更新日志

## 🎉 新功能

### 布尔搜索运算符支持

现在支持使用 AND/OR/NOT 运算符进行复杂搜索！

#### 支持的运算符

**AND - 必须同时包含**
```
搜索: 雁 AND 塔
结果: 包含"雁"且包含"塔"的文件
```

**OR - 包含任一即可**
```
搜索: 雁 OR 曲
结果: 包含"雁"或包含"曲"的文件
```

**NOT - 排除特定词**
```
搜索: 雁 NOT 塔
结果: 包含"雁"但不包含"塔"的文件
```

#### 组合使用示例

**复杂查询**：
```
搜索: 西安 AND 雁塔 NOT 临时
结果: 同时包含"西安"和"雁塔"，但不包含"临时"的文件
```

**OR 查询**：
```
搜索: 雁塔 OR 曲江 OR 高新
结果: 包含任一区域名称的文件
```

### 技术实现

**查询解析**：
1. 检测查询字符串中的 AND/OR/NOT 关键词（不区分大小写）
2. 将查询拆分成多个条件
3. 生成相应的 SQL LIKE 条件组合

**SQL 示例**：
```sql
-- 查询: "雁 AND 塔"
SELECT * FROM files WHERE 
  (content LIKE '%雁%' OR file_name LIKE '%雁%') 
  AND 
  (content LIKE '%塔%' OR file_name LIKE '%塔%')

-- 查询: "雁 OR 曲"  
SELECT * FROM files WHERE 
  (content LIKE '%雁%' OR file_name LIKE '%雁%') 
  OR 
  (content LIKE '%曲%' OR file_name LIKE '%曲%')

-- 查询: "雁 NOT 塔"
SELECT * FROM files WHERE 
  (content LIKE '%雁%' OR file_name LIKE '%雁%') 
  AND 
  (content NOT LIKE '%塔%' AND file_name NOT LIKE '%塔%')
```

## 📝 使用说明

### 运算符规则

1. **不区分大小写**
   - `AND`, `and`, `And` 都可以
   - `OR`, `or`, `Or` 都可以
   - `NOT`, `not`, `Not` 都可以

2. **空格重要**
   - ✅ 正确：`雁 AND 塔`（AND 前后有空格）
   - ❌ 错误：`雁AND塔`（会被当作普通文本）

3. **优先级**
   - NOT > AND > OR
   - 暂不支持括号分组

4. **不能混用 AND 和 OR**
   - ✅ 支持：`term1 AND term2 AND term3`
   - ✅ 支持：`term1 OR term2 OR term3`
   - ✅ 支持：`term1 AND term2 NOT term3`
   - ❌ 不支持：`term1 AND term2 OR term3`（会被当作 OR 查询）

### 测试示例

**简单搜索**（无运算符）：
```
输入: 雁塔
结果: 所有包含"雁塔"的文件
```

**AND 搜索**：
```
输入: 雁 AND 塔
结果: 同时包含"雁"和"塔"的文件（不一定相邻）
```

**OR 搜索**：
```
输入: doc OR pdf
结果: 包含"doc"或"pdf"的文件
```

**NOT 搜索**：
```
输入: 雁 NOT 塔
结果: 包含"雁"但不包含"塔"的文件
```

## ⚙️ 技术细节

### 代码修改

**修改文件**：
- `client/file_indexer.h` - 添加布尔查询方法声明
- `client/file_indexer.cpp` - 实现布尔查询解析和执行
- `CMakeLists.txt` - 版本号升级到 v1.5.0

**新增函数**：
```cpp
// 检测和执行布尔查询
QStringList searchWithBoolean(const QString& query, const QStringList& fileTypes);

// 解析运算符
// 提取 AND/OR/NOT 条件
// 构建 SQL 查询
```

### 日志输出

启用布尔搜索时的日志：
```
[FileIndexer] Detected boolean operators, using boolean search
[FileIndexer] Parsing boolean query: 雁 AND 塔
[FileIndexer]   AND term: 雁
[FileIndexer]   AND term: 塔
[FileIndexer] Boolean SQL: SELECT DISTINCT f.file_path, f.file_name FROM files f...
[FileIndexer]   Found: xxx.doc
[FileIndexer] Boolean search completed: X results
```

## 📊 性能影响

| 查询类型 | 复杂度 | 性能 |
|----------|--------|------|
| 简单搜索 | 1个 LIKE | 快 ✅ |
| AND 查询 | N个 LIKE (AND) | 中等 ⚠️ |
| OR 查询 | N个 LIKE (OR) | 慢 ⚠️ |
| NOT 查询 | +N个 NOT LIKE | 中等 ⚠️ |

**优化建议**：
- 文件少于 1000 个时性能完全可接受
- 避免使用过多 OR 条件（每个 OR 增加一次全表扫描）
- NOT 条件放在最后（先过滤出候选集）

## 🔄 从 v1.4.1 升级

**自动兼容**：
- 旧的搜索查询（无运算符）继续正常工作
- 新的布尔查询自动启用
- 无需重建索引

**新功能测试**：
1. 搜索 "雁 AND 塔" → 应该有结果
2. 搜索 "doc OR pdf" → 应该找到所有文档
3. 搜索 "雁 NOT 塔" → 只有包含"雁"但不含"塔"的文件

## 🚧 已知限制

1. **不支持括号**
   - ❌ `(雁 OR 曲) AND 江` 不支持

2. **不能混用 AND 和 OR**
   - ❌ `雁 AND 塔 OR 曲` 会被解析为 OR 查询
   - ✅ 解决：分两次搜索

3. **空格敏感**
   - 运算符前后必须有空格
   - `雁AND塔` 会被当作普通文本 "雁AND塔"

4. **Web搜索**
   - 暂时仍为"客户端专用"功能
   - 计划在下个版本实现

## 📚 相关文档

- [v1.4.1 更新日志](CHANGELOG_v1.4.1.md) - LIKE 查询实现
- [全文检索功能状态](CONTENT_SEARCH_FINAL_STATUS.md)

## 🎯 下一步计划

- 🔮 v1.5.1: Web端全文搜索（服务器广播机制）
- 🔮 v1.6.0: 支持括号分组和复杂布尔表达式
- 🔮 v1.7.0: 搜索结果高亮显示

---

**发布日期**：2026-08-31  
**版本号**：v1.5.0  
**分支**：main  
**上一版本**：v1.4.1
