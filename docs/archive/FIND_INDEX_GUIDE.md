# 如何找到索引文件

## 快速方法

### 方法 1：运行查找脚本（推荐）

```powershell
.\find_index_new.ps1
```

输出示例：
```
=== CrossNetShare Index Location Finder ===

1. Running Client
   Executable: E:\dev\crossnet-share\build\Release\CrossNetShareClient.exe
   Directory: E:\dev\crossnet-share\build\Release

2. Index File Location
   Path: E:\dev\crossnet-share\build\Release\content_index.db

   Status: EXISTS
   Size: 0.15 MB (152.5 KB)
   Created: 2026-09-01 21:00:00
   Last Modified: 2026-09-01 21:05:30
   Activity: ACTIVE (updated 2.3 min ago)

   You can open this location in Explorer:
   explorer.exe "E:\dev\crossnet-share\build\Release"

3. Related Files in Same Directory
   ⚙️ CrossNetShareClient.exe (2.5 MB)
   📊 content_index.db (152.5 KB)
   🔧 simple.dll (1.2 MB)
   🔧 sqlite3.dll (3.4 MB)
```

### 方法 2：通过进程找到目录

```powershell
# 一行命令
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue; if ($proc) { Split-Path $proc.Path } else { "Not running" }
```

### 方法 3：直接打开目录

```powershell
# 找到并打开包含索引文件的目录
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
if ($proc) {
    $dir = Split-Path $proc.Path
    explorer.exe $dir
}
```

## 索引文件位置规则

### ✅ v2.0.1 及以后（当前版本）
```
索引文件与客户端程序在同一目录
<客户端.exe 所在目录>\content_index.db
```

**示例**：
- 开发构建：`E:\dev\crossnet-share\build\Release\content_index.db`
- 发布版本：`C:\Program Files\CrossNetShare\content_index.db`
- 便携版本：`D:\Apps\CrossNetShare\content_index.db`

### ❌ v2.0.0（旧版本）
```
C:\Users\<用户名>\AppData\Local\CrossNetShareClient\content_index.db
```

## 验证索引文件是否工作

### 检查 1：文件存在
```powershell
# 假设客户端在 E:\dev\crossnet-share\build\Release
Test-Path "E:\dev\crossnet-share\build\Release\content_index.db"
# 应该返回: True
```

### 检查 2：文件大小
```powershell
$dbPath = "E:\dev\crossnet-share\build\Release\content_index.db"
if (Test-Path $dbPath) {
    $size = (Get-Item $dbPath).Length / 1KB
    Write-Host "Index size: $([math]::Round($size, 2)) KB"
}
# 应该 > 0 KB（通常几十到几百 KB）
```

### 检查 3：最近修改时间
```powershell
$dbPath = "E:\dev\crossnet-share\build\Release\content_index.db"
if (Test-Path $dbPath) {
    $file = Get-Item $dbPath
    $age = (Get-Date) - $file.LastWriteTime
    Write-Host "Last updated: $([math]::Round($age.TotalMinutes, 1)) minutes ago"
}
# 如果正在索引，应该是最近修改的（< 10 分钟）
```

## 常见场景

### 场景 1：首次运行客户端
```
时间线：
T+0s   : 启动客户端
T+1-2s : 连接服务器
T+3-5s : 索引器初始化
T+5-10s: 创建 content_index.db（文件出现！）
T+10-60s: 索引所有文件
```

**如何确认**：
```powershell
# 等待 10 秒后运行
Start-Sleep -Seconds 10
.\find_index_new.ps1
```

### 场景 2：开发环境
```
构建目录：E:\dev\crossnet-share\build\Release\
索引位置：E:\dev\crossnet-share\build\Release\content_index.db
```

**快速检查**：
```powershell
dir "E:\dev\crossnet-share\build\Release\*.db"
```

### 场景 3：生产环境
```
安装目录：C:\Program Files\CrossNetShare\
索引位置：C:\Program Files\CrossNetShare\content_index.db
```

**注意**：可能需要管理员权限写入

### 场景 4：便携版本
```
U盘目录：U:\CrossNetShare\
索引位置：U:\CrossNetShare\content_index.db
```

可以整个文件夹复制到其他机器使用！

## 疑难解答

### 问题 1：找不到索引文件

**检查步骤**：
1. 确认客户端正在运行：`Get-Process CrossNetShareClient`
2. 确认已连接服务器（客户端窗口显示"已连接"）
3. 确认配置了共享路径
4. 等待 5 分钟后再检查
5. 查看客户端日志输出

### 问题 2：索引文件存在但没有内容

```powershell
# 检查文件大小
$dbPath = "<替换为你的路径>\content_index.db"
(Get-Item $dbPath).Length
# 如果是 0 字节，说明索引还没开始
```

**解决方法**：
- 等待更长时间（首次索引需要几分钟）
- 检查共享文件夹是否有支持的文件（.doc, .docx, .txt, .pdf）
- 重启客户端

### 问题 3：权限错误

如果客户端在 `Program Files` 等受保护目录：

**方案 A**：以管理员身份运行
```powershell
# 右键客户端 → 以管理员身份运行
```

**方案 B**：移动到非受保护目录
```powershell
# 将客户端移动到：
C:\Users\<你的用户名>\CrossNetShare\
# 或
D:\Apps\CrossNetShare\
```

## 删除索引文件重建

如果索引损坏或想重新索引：

```powershell
# 1. 停止客户端
Stop-Process -Name CrossNetShareClient -Force

# 2. 找到并删除索引文件
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
$dir = Split-Path $proc.Path  # 如果已关闭，手动指定目录
Remove-Item "$dir\content_index.db" -Force

# 3. 重启客户端
# 会自动重新创建索引
```

## 脚本汇总

### 完整诊断脚本

```powershell
# 保存为 diagnose_index.ps1
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue

if (-not $proc) {
    Write-Host "ERROR: Client not running" -ForegroundColor Red
    exit 1
}

$exePath = $proc.Path
$dir = Split-Path $exePath
$dbPath = Join-Path $dir "content_index.db"

Write-Host "=== Index Diagnostics ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Client EXE: $exePath"
Write-Host "Client DIR: $dir"
Write-Host "Index Path: $dbPath"
Write-Host ""

if (Test-Path $dbPath) {
    $file = Get-Item $dbPath
    $sizeKB = [math]::Round($file.Length / 1KB, 2)
    $age = (Get-Date) - $file.LastWriteTime
    
    Write-Host "Status: EXISTS" -ForegroundColor Green
    Write-Host "Size: $sizeKB KB"
    Write-Host "Age: $([math]::Round($age.TotalMinutes, 1)) minutes"
    
    if ($sizeKB -eq 0) {
        Write-Host "WARNING: File is empty (0 KB)" -ForegroundColor Yellow
    } elseif ($age.TotalMinutes -gt 60) {
        Write-Host "WARNING: Not updated for over an hour" -ForegroundColor Yellow
    } else {
        Write-Host "Status: HEALTHY" -ForegroundColor Green
    }
} else {
    Write-Host "Status: NOT FOUND" -ForegroundColor Red
    Write-Host "Action: Wait 1-5 minutes or check logs"
}
```

### 自动打开索引目录

```powershell
# 保存为 open_index_dir.ps1
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
if ($proc) {
    $dir = Split-Path $proc.Path
    explorer.exe $dir
    Write-Host "Opened: $dir"
} else {
    Write-Host "Client not running" -ForegroundColor Red
}
```

## 总结

**简单记忆**：
> 索引文件和客户端程序在同一个文件夹里

**查找方法**（按难易排序）：
1. 运行 `.\find_index_new.ps1`
2. 查看客户端日志输出
3. 找到客户端.exe所在文件夹
4. 在该文件夹找 `content_index.db`

**预期结果**：
- 文件大小：几十到几百 KB
- 文件位置：与 `CrossNetShareClient.exe` 同目录
- 创建时间：首次连接后 1-5 分钟
