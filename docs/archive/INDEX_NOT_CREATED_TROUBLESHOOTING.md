# 索引文件未创建问题排查

## 问题描述
索引文件 `content_index.db` 未在预期位置创建。

## 正确的索引文件路径

### ✅ 实际路径（已确认）
```
C:\Users\<用户名>\AppData\Local\CrossNetShareClient\content_index.db
```

**注意**：文件夹名是 `CrossNetShareClient`，不是 `CrossNetShare`

### 为什么是 CrossNetShareClient？
因为客户端可执行文件名为 `CrossNetShareClient.exe`：
```cmake
# CMakeLists.txt:124
add_executable(CrossNetShareClient WIN32 ...)
```

Qt 的 `QStandardPaths::AppDataLocation` 使用可执行文件名作为子目录名。

## 当前状态检查

### 1. 共享路径
```
路径: C:\Users\asusu\Desktop\1212
状态: ✅ 存在
文件数量: 9 个
  - .docx: 1 个
  - .doc:  1 个
  - .pdf:  2 个
  - .txt:  5 个
```

### 2. 客户端配置
```json
{
  "autoReconnect": true,
  "clientId": "B",
  "serverHost": "127.0.0.1",
  "serverPort": 8888,
  "sharePath": "C:/Users/asusu/Desktop/1212"
}
```

### 3. 索引文件
```
路径: C:\Users\asusu\AppData\Local\CrossNetShareClient\content_index.db
状态: ❌ 不存在
```

## 索引未创建的可能原因

### 原因 1: 客户端未连接到服务器
索引功能只在客户端成功连接到服务器后才启动。

**检查方法**：
```powershell
# 查看客户端日志
Get-Content "$env:LOCALAPPDATA\CrossNetShareClient\startup.log" -Tail 50
```

**预期日志**：
```
[Client] Connected to server: 127.0.0.1:8888
[Client] Registered as: B
[FileIndexer] Initializing with shared path: C:/Users/asusu/Desktop/1212
```

### 原因 2: 索引器初始化失败
可能是 SQLite DLL 缺失或数据库创建失败。

**检查方法**：
```powershell
# 检查 SQLite DLL
Test-Path "E:\dev\crossnet-share\build\Release\sqlite3.dll"
Test-Path "E:\dev\crossnet-share\build\Release\simple.dll"
```

**预期日志**：
```
[FileIndexer] Opening database with SQLite C API...
[FileIndexer] Database opened successfully
[FileIndexer] FTS5 extension loaded successfully
```

### 原因 3: 客户端还未启动索引扫描
索引是在客户端注册成功后异步启动的。

**预期流程**：
1. 客户端连接服务器（1 秒）
2. 注册客户端 ID（< 1 秒）
3. 初始化索引器（1-2 秒）
4. 开始全量扫描（3-10 分钟）

### 原因 4: 索引正在进行中
首次索引需要时间，数据库文件在第一次写入时才创建。

**检查方法**：
- 等待 1-2 分钟后再次检查
- 查看客户端窗口日志输出

## 解决步骤

### 步骤 1: 确保服务器运行
```powershell
# 检查服务器是否在监听
netstat -an | findstr "8888"
```

预期输出：
```
TCP    0.0.0.0:8888           0.0.0.0:0              LISTENING
```

### 步骤 2: 启动客户端
1. 运行 `CrossNetShareClient.exe`
2. 等待窗口显示"已连接"或"已注册"
3. 查看日志输出

### 步骤 3: 等待索引初始化
索引会在连接成功后自动启动，首次需要 1-5 分钟。

**实时检查索引文件**：
```powershell
# 每 5 秒检查一次
while ($true) {
    Clear-Host
    $path = "$env:LOCALAPPDATA\CrossNetShareClient\content_index.db"
    if (Test-Path $path) {
        $file = Get-Item $path
        Write-Host "Index file EXISTS!" -ForegroundColor Green
        Write-Host "Size: $([math]::Round($file.Length/1KB, 2)) KB"
        Write-Host "Last Modified: $($file.LastWriteTime)"
        break
    } else {
        Write-Host "Waiting for index file..." -ForegroundColor Yellow
        Write-Host "Checked at: $(Get-Date -Format 'HH:mm:ss')"
    }
    Start-Sleep -Seconds 5
}
```

### 步骤 4: 验证索引完成
```powershell
# 运行验证脚本
.\check_index_simple.ps1
```

预期输出：
```
2. Index File
   Path: C:\Users\asusu\AppData\Local\CrossNetShareClient\content_index.db
   Status: EXISTS
   Size: 0.15 MB
   Last Modified: 2026-09-01 20:30:00
```

### 步骤 5: 测试全文搜索
```powershell
# 运行 API 测试
.\test_web_search.ps1
```

或在浏览器访问：
```
http://localhost:8080/browse
```

## 索引创建时间线

对于 9 个小文件：

```
T+0s    : 客户端启动
T+1s    : 连接服务器成功
T+2s    : 注册客户端 ID
T+3s    : 初始化索引器
T+4s    : 创建数据库文件 (content_index.db 出现)
T+5-10s : 索引第一个文件
T+10-60s: 索引所有 9 个文件
T+60s   : 首次索引完成
```

## 手动触发索引

如果等待很久仍然没有索引文件，可以尝试：

### 方法 1: 重启客户端
1. 关闭客户端
2. 删除旧配置（可选）
3. 重新启动客户端
4. 重新连接服务器

### 方法 2: 手动创建数据库文件
```powershell
# 如果怀疑是权限问题，手动测试创建文件
$testFile = "$env:LOCALAPPDATA\CrossNetShareClient\test.db"
"test" | Out-File $testFile
Remove-Item $testFile
Write-Host "File creation test: OK"
```

### 方法 3: 检查 SQLite 依赖
```powershell
# 确保 SQLite DLL 在可执行文件目录
$exeDir = Split-Path (Get-Process CrossNetShareClient -ErrorAction SilentlyContinue).Path
if ($exeDir) {
    Get-ChildItem "$exeDir\*.dll" | Where-Object { $_.Name -match "sqlite|simple" }
}
```

## 预期的索引性能（9 个文件）

| 指标 | 预期值 |
|------|--------|
| 首次索引时间 | 10-60 秒 |
| 索引文件大小 | 50-200 KB |
| 索引后搜索速度 | < 10ms |
| 内存占用 | < 50 MB |

## 调试日志检查

### 查看完整启动日志
```powershell
Get-Content "$env:LOCALAPPDATA\CrossNetShareClient\startup.log"
```

### 查找索引相关日志
```powershell
Get-Content "$env:LOCALAPPDATA\CrossNetShareClient\startup.log" | 
    Select-String "FileIndexer|Indexed|FTS5|SQLite"
```

### 查找错误日志
```powershell
Get-Content "$env:LOCALAPPDATA\CrossNetShareClient\startup.log" | 
    Select-String "error|failed|ERROR|FAILED" -Context 2
```

## 成功标志

当索引成功创建后，你会看到：

### ✅ 文件系统
```
C:\Users\asusu\AppData\Local\CrossNetShareClient\
├── crossnet_client_config.json  (配置文件)
├── content_index.db             (索引文件 ← 这个出现说明成功)
└── startup.log                  (日志文件)
```

### ✅ 日志输出
```
[FileIndexer] Successfully initialized for path: C:/Users/asusu/Desktop/1212
[FileIndexer] Starting full scan...
[FileIndexer] Indexed: 文件1.docx (1/9)
[FileIndexer] Indexed: 文件2.doc (2/9)
...
[FileIndexer] Full scan completed: 9 files indexed in 15.3s
```

### ✅ 功能验证
```powershell
# API 搜索测试成功
.\test_web_search.ps1
# 输出: ✓ 搜索成功, 结果数量: 4
```

```
# Web 界面搜索成功
http://localhost:8080/browse
搜索 "测试关键词" → 找到 X 个文件
```

## 联系支持

如果以上步骤都无法解决问题，请提供：

1. 客户端完整日志：`startup.log`
2. 配置文件：`crossnet_client_config.json`
3. 索引检查输出：`.\check_index_simple.ps1` 的结果
4. SQLite DLL 检查：是否存在 `sqlite3.dll` 和 `simple.dll`
5. 客户端版本：构建日期和 commit hash

## 快速诊断命令

复制粘贴运行：

```powershell
Write-Host "=== Quick Diagnostic ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Index file:" -ForegroundColor Yellow
if (Test-Path "$env:LOCALAPPDATA\CrossNetShareClient\content_index.db") {
    Write-Host "   EXISTS" -ForegroundColor Green
} else {
    Write-Host "   NOT FOUND" -ForegroundColor Red
}
Write-Host ""
Write-Host "2. Share path:" -ForegroundColor Yellow
$config = Get-Content "$env:LOCALAPPDATA\CrossNetShareClient\crossnet_client_config.json" | ConvertFrom-Json
Write-Host "   $($config.sharePath)"
if (Test-Path $config.sharePath) {
    $count = (Get-ChildItem $config.sharePath -Recurse -File).Count
    Write-Host "   EXISTS ($count files)" -ForegroundColor Green
} else {
    Write-Host "   NOT FOUND" -ForegroundColor Red
}
Write-Host ""
Write-Host "3. Server connection:" -ForegroundColor Yellow
$serverPort = $config.serverPort
if (netstat -an | Select-String "$serverPort.*ESTABLISHED") {
    Write-Host "   CONNECTED" -ForegroundColor Green
} else {
    Write-Host "   NOT CONNECTED" -ForegroundColor Red
}
```

## 总结

- ✅ **正确路径**：`C:\Users\<用户名>\AppData\Local\CrossNetShareClient\`
- ✅ **共享路径已配置**：9 个文件等待索引
- ❌ **索引文件未创建**：需要启动客户端并等待初始化
- ⏳ **预期等待时间**：连接成功后 1-5 分钟

**下一步**：
1. 确保服务器运行
2. 启动客户端
3. 等待 2-5 分钟
4. 运行 `.\check_index_simple.ps1` 验证
