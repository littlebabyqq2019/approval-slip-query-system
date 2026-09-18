# 如何获取崩溃日志

## 问题
程序崩溃太快，来不及复制日志窗口中的内容。

## 解决方案
程序现在会自动将所有日志保存到文件中！

## 日志文件位置

### 默认位置
```
<客户端程序所在目录>\client_log.txt
```

### 示例
如果你的客户端程序在：
```
D:\Program Files\CrossNetShare-Windows-x64\Client\CrossNetShareClient.exe
```

那么日志文件在：
```
D:\Program Files\CrossNetShare-Windows-x64\Client\client_log.txt
```

## 如何查看日志

### 方法 1：直接打开
```
1. 打开客户端程序所在的文件夹
2. 找到 client_log.txt 文件
3. 用记事本打开
```

### 方法 2：使用命令
```cmd
cd "D:\Program Files\CrossNetShare-Windows-x64\Client"
notepad client_log.txt
```

### 方法 3：崩溃后立即查看
```powershell
# 显示最后50行日志
Get-Content "D:\Program Files\CrossNetShare-Windows-x64\Client\client_log.txt" -Tail 50
```

## 日志内容示例

正常运行的日志：
```
=== Session started at 2026-09-02 17:00:00 ===
[2026-09-02 17:00:00] Configuration loaded successfully
[2026-09-02 17:00:01] Connected to server
[2026-09-02 17:00:02] Performing incremental index update...
[2026-09-02 17:00:30] Content indexing finished
[2026-09-02 17:00:30] Indexed 6312 files, total size: 150 MB
```

崩溃前的日志（关键信息）：
```
[2026-09-02 17:00:30] Content indexing finished
[2026-09-02 17:00:30] Indexed 6312 files, total size: 150 MB
<程序在这里崩溃，没有更多日志>
```

## 需要提供的信息

如果程序崩溃，请提供：

### 1. 日志文件的最后部分
```powershell
# 复制最后50行
Get-Content "client_log.txt" -Tail 50 | clip
# 现在日志已经在剪贴板，可以粘贴
```

### 2. 完整的日志文件（如果可以）
- 将 `client_log.txt` 压缩成 zip
- 发送给开发者

### 3. 崩溃时间
- 记录程序崩溃的确切时间
- 这样可以在日志中找到对应位置

## 日志特点

### 实时写入
- 每条日志都立即写入文件
- 即使程序崩溃也能看到崩溃前的日志

### 追加模式
- 每次启动程序都追加到同一个文件
- 可以看到历史运行记录
- 每次启动都有分隔符：`=== Session started ===`

### 文件大小
- 日志文件会逐渐变大
- 可以定期删除或重命名
- 不影响程序运行

## 清理日志文件

如果日志文件太大：
```cmd
# 删除旧日志
del "D:\Program Files\CrossNetShare-Windows-x64\Client\client_log.txt"

# 或者重命名备份
ren "client_log.txt" "client_log_backup_2026-09-02.txt"
```

下次启动程序会自动创建新的日志文件。

## 诊断步骤

### 1. 重现崩溃
```
1. 删除旧的 client_log.txt（可选）
2. 启动客户端
3. 等待崩溃
4. 立即查看 client_log.txt
```

### 2. 查找关键信息
在日志中查找：
- 最后一条日志是什么？
- 是否有 "ERROR" 或 "FATAL" 字样？
- 是否有异常信息？

### 3. 提供反馈
将以下信息提供给开发者：
- 最后 50-100 行日志
- 崩溃前做了什么操作
- 崩溃是否可重现

## 常见日志模式

### 模式 1：索引完成后崩溃
```
[时间] Content indexing finished
[时间] Indexed XXXX files
<崩溃>
```
**诊断**：索引后处理有问题

### 模式 2：启动立即崩溃
```
[时间] Configuration loaded
<崩溃>
```
**诊断**：初始化过程有问题

### 模式 3：随机崩溃
```
[时间] 正常日志
[时间] 正常日志
<崩溃>
```
**诊断**：需要查看崩溃前的操作

## 高级调试

### 查看特定时间段的日志
```powershell
# 查找特定时间
Select-String -Path "client_log.txt" -Pattern "17:00" -Context 5,5
```

### 查找错误
```powershell
# 查找所有错误
Select-String -Path "client_log.txt" -Pattern "ERROR|FATAL|Exception"
```

### 统计会话数
```powershell
# 看看程序启动了多少次
Select-String -Path "client_log.txt" -Pattern "Session started" | Measure-Object
```

## 技术细节

### 日志格式
```
[YYYY-MM-DD HH:mm:ss] 日志消息
```

### 编码
- UTF-8
- 包含中文

### 刷新策略
- 每条日志都立即 `flush()`
- 确保崩溃前的日志被写入

### 线程安全
- 日志写入是线程安全的
- 使用 `static` 确保单例

## 注意事项

1. **不要在程序运行时删除日志文件**
   - 可能导致写入失败
   - 先关闭程序再删除

2. **日志文件可能很大**
   - 长时间运行会积累大量日志
   - 定期清理即可

3. **日志包含敏感信息**
   - 可能包含文件路径
   - 分享时注意隐私

## 示例：完整的诊断流程

```powershell
# 1. 进入程序目录
cd "D:\Program Files\CrossNetShare-Windows-x64\Client"

# 2. 备份旧日志（可选）
if (Test-Path "client_log.txt") {
    Copy-Item "client_log.txt" "client_log_old.txt"
}

# 3. 清空日志重新开始
Remove-Item "client_log.txt" -ErrorAction SilentlyContinue

# 4. 启动程序
.\CrossNetShareClient.exe

# 5. 等待崩溃后查看日志
notepad client_log.txt

# 6. 或者复制最后50行到剪贴板
Get-Content "client_log.txt" -Tail 50 | clip
```

---

**最后更新**：2026-09-02  
**版本要求**：v2.2.2 或更新（包含日志文件功能）
