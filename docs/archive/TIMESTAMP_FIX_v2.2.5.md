# 时间显示格式修复 - v2.2.5

## 问题

Web 界面显示时间不一致：

**文件列表** (正确):
```
创建时间: 2026-09-04 15:43:29
```

**全文搜索结果** (错误):
```
创建时间: 1788507809
```

## 原因

全文搜索 API 返回的是原始 Unix 时间戳（秒），没有格式化。

## 修复

在服务端 `handleContentSearch()` 函数中格式化时间戳：

```cpp
// 修改前
item["modifyTime"] = result.modifyTime;  // 返回数字 1788507809

// 修改后  
QDateTime dateTime = QDateTime::fromSecsSinceEpoch(result.modifyTime);
item["modifyTime"] = dateTime.toString("yyyy-MM-dd HH:mm:ss").toStdString();
// 返回字符串 "2026-09-04 15:43:29"
```

## 效果

现在全文搜索结果和文件列表使用相同的时间显示格式，用户体验一致。

## 文件

- `server/web_server.cpp`: 修改 `handleContentSearch()` 函数
- `CMakeLists.txt`: 版本号升级到 2.2.5

## 提交

- **Commit**: d227ce2
- **Version**: 2.2.5
- **Date**: 2026-09-04

---

修复完成！重新编译服务端即可。
