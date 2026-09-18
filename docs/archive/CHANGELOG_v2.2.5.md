# CrossNetShare v2.2.5 更新日志

**发布日期**: 2026-09-04

## 🐛 Bug 修复

### 修复 Web 界面全文搜索结果的时间显示格式

**问题描述**:
- Web 界面左侧文件列表显示的创建时间格式正确：`2026-09-04 15:43:29`
- 但全文搜索结果显示的创建时间是原始的 Unix 时间戳：`1788507809`
- 时间戳无法直观理解，用户体验不佳

**根本原因**:
- 全文搜索 API (`/api/content-search`) 返回的 `modifyTime` 字段直接使用 Unix 时间戳（秒）
- 前端没有做时间格式化处理
- 而文件列表 API 返回的是已格式化的时间字符串

**修复方案**:
- 在服务端 API 返回前将 Unix 时间戳格式化为可读的时间字符串
- 使用 `QDateTime::fromSecsSinceEpoch()` 转换时间戳
- 格式化为 `"YYYY-MM-DD HH:MM:SS"` 格式
- 与文件列表的时间显示格式保持一致

**修改文件**:
- `server/web_server.cpp`: `handleContentSearch()` 函数

**修改代码**:
```cpp
// 修改前
item["modifyTime"] = result.modifyTime;

// 修改后
QDateTime dateTime = QDateTime::fromSecsSinceEpoch(result.modifyTime);
item["modifyTime"] = dateTime.toString("yyyy-MM-dd HH:mm:ss").toStdString();
```

## 用户体验改进

**改进前**:
```
创建时间: 1788507809
```
→ 无法直观理解这是什么时间

**改进后**:
```
创建时间: 2026-09-04 15:43:29
```
→ 一目了然的日期和时间

## 技术细节

### Unix 时间戳

Unix 时间戳（Unix timestamp）：
- 从 1970-01-01 00:00:00 UTC 开始计算的秒数
- 例如：`1788507809` = 2026-09-04 15:43:29 (本地时间)
- 便于存储和计算，但不便于阅读

### 时间格式化

使用 Qt 的 `QDateTime` 类进行格式化：

```cpp
// 1. 从时间戳创建 QDateTime 对象
QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);

// 2. 格式化为字符串
QString formatted = dateTime.toString("yyyy-MM-dd HH:mm:ss");

// 格式说明：
// yyyy - 4位年份 (2026)
// MM   - 2位月份 (09)
// dd   - 2位日期 (04)
// HH   - 24小时制小时 (15)
// mm   - 2位分钟 (43)
// ss   - 2位秒数 (29)
```

### 时区处理

`QDateTime::fromSecsSinceEpoch()` 默认使用本地时区：
- 服务器在哪个时区，就显示哪个时区的时间
- 如果需要 UTC 时间，可以使用：
  ```cpp
  QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);
  ```

### 一致性

现在所有 Web API 返回的时间字段都使用相同格式：
- `/api/files` - 文件列表
- `/api/content-search` - 全文搜索结果
- 格式：`"YYYY-MM-DD HH:MM:SS"`

## 相关接口

### /api/content-search

**响应格式**:
```json
{
  "success": true,
  "query": "搜索关键词",
  "totalResults": 1,
  "results": [
    {
      "filename": "2026-文件批办单#6776-(E-26-260).docx",
      "relativePath": "2026-文件批办单#6776-(E-26-260).docx",
      "ownerClient": "收文",
      "size": 11878,
      "modifyTime": "2026-09-04 15:43:29"  // 改进：现在是格式化的字符串
    }
  ]
}
```

**改进前**:
```json
"modifyTime": 1788507809  // Unix 时间戳
```

**改进后**:
```json
"modifyTime": "2026-09-04 15:43:29"  // 可读的时间字符串
```

## 向后兼容

此改进**不完全向后兼容**：
- 之前 API 返回的是数字类型（Unix 时间戳）
- 现在返回的是字符串类型（格式化时间）
- 如果前端代码有对 `modifyTime` 进行数值计算，需要调整

**影响评估**:
- 当前前端代码只是显示时间，不进行计算
- 因此这个改动不会影响现有功能
- 反而改善了用户体验

## 测试建议

1. **测试全文搜索**:
   - 在 Web 界面进行全文搜索
   - 检查搜索结果的创建时间格式
   - 应该显示为 `YYYY-MM-DD HH:MM:SS` 格式

2. **对比文件列表**:
   - 对比左侧文件列表和搜索结果的时间显示
   - 两者应该使用相同的格式

3. **不同时区测试**:
   - 在不同时区的服务器上测试
   - 确认时间显示符合本地时区

## 升级说明

直接替换 `CrossNetShareServer.exe` 即可，无需修改配置或清除数据。

## 相关问题

此修复同时解决了：
- 时间显示不一致的问题
- Unix 时间戳不直观的问题
- 用户体验较差的问题

---

**版本**: v2.2.5
**上一版本**: v2.2.4
**提交**: (待生成)
