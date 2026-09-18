# 索引文件路径修改说明

## 修改内容

将索引文件保存位置从系统 AppData 目录改为客户端可执行文件所在目录。

## 修改原因

1. **更容易找到**：索引文件和客户端在同一目录，方便查看和管理
2. **便于调试**：开发和测试时可以直接看到索引文件
3. **方便备份**：备份客户端程序时可以一起备份索引
4. **用户体验**：用户可以直接删除索引文件重新索引，无需找隐藏的 AppData 目录

## 新的索引文件路径

### ✅ 修改后（v2.0.1+）
```
<客户端程序目录>/content_index.db
```

**示例**（假设客户端在 `C:\Program Files\CrossNetShare\`）：
```
C:\Program Files\CrossNetShare\CrossNetShareClient.exe
C:\Program Files\CrossNetShare\content_index.db  ← 索引文件在这里
C:\Program Files\CrossNetShare\sqlite3.dll
C:\Program Files\CrossNetShare\simple.dll
```

**示例**（开发构建）：
```
E:\dev\crossnet-share\build\Release\CrossNetShareClient.exe
E:\dev\crossnet-share\build\Release\content_index.db  ← 索引文件在这里
```

### ❌ 修改前（v2.0.0）
```
C:\Users\<用户名>\AppData\Local\CrossNetShareClient\content_index.db
```

## 代码修改

### 文件：`client/ui/main_window.cpp`

**修改前**：
```cpp
// 设置索引数据库路径
QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
QDir().mkpath(appDataPath);
QString dbPath = appDataPath + "/content_index.db";

onLogMessage(QString("Database path: %1").arg(dbPath));
onLogMessage(QString("App data path exists: %1").arg(QDir(appDataPath).exists() ? "yes" : "no"));
```

**修改后**：
```cpp
// 设置索引数据库路径 - 保存在客户端可执行文件所在目录
QString appDir = QCoreApplication::applicationDirPath();
QString dbPath = appDir + "/content_index.db";

onLogMessage(QString("Database path: %1").arg(dbPath));
onLogMessage(QString("Application directory: %1").arg(appDir));
```

## 影响和注意事项

### ✅ 优点
1. **可见性**：索引文件和程序在一起，一目了然
2. **便携性**：可以直接复制整个程序文件夹（包括索引）到其他机器
3. **调试友好**：开发时可以直接查看索引文件大小和修改时间
4. **清理简单**：卸载程序时可以直接删除整个文件夹

### ⚠️ 注意事项
1. **权限问题**：如果程序安装在 `Program Files` 等受保护目录，可能需要管理员权限写入
   - **建议**：将程序安装在用户目录或非受保护位置
   - **或者**：首次运行时以管理员身份运行

2. **多用户共享**：同一台机器的不同用户会使用不同的索引文件
   - 这通常是期望的行为（每个用户有自己的索引）

3. **兼容性**：现有用户升级后，旧的索引文件仍在 AppData 中
   - **不会自动迁移**，会创建新索引
   - 如果需要，可以手动复制旧索引文件到新位置

## 如何查找索引文件

### 方法 1：通过客户端日志
启动客户端后，查看日志输出：
```
Database path: E:\dev\crossnet-share\build\Release\content_index.db
Application directory: E:\dev\crossnet-share\build\Release
```

### 方法 2：直接查看程序目录
1. 找到 `CrossNetShareClient.exe` 所在的文件夹
2. 在该文件夹中找 `content_index.db`

### 方法 3：PowerShell 快速查找
```powershell
# 找到客户端进程的目录
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
if ($proc) {
    $dir = Split-Path $proc.Path
    Write-Host "Client directory: $dir"
    $dbPath = Join-Path $dir "content_index.db"
    if (Test-Path $dbPath) {
        Write-Host "Index file EXISTS: $dbPath"
        Get-Item $dbPath | Select-Object Name, Length, LastWriteTime
    } else {
        Write-Host "Index file NOT YET CREATED"
    }
}
```

## 验证索引文件位置

### 启动客户端后运行：
```powershell
# 创建快速检查脚本
$script = @'
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
if ($proc) {
    $exePath = $proc.Path
    $dir = Split-Path $exePath
    $dbPath = Join-Path $dir "content_index.db"
    
    Write-Host "=== Index File Location ===" -ForegroundColor Cyan
    Write-Host "Client EXE: $exePath"
    Write-Host "Client DIR: $dir"
    Write-Host "Index Path: $dbPath"
    Write-Host ""
    
    if (Test-Path $dbPath) {
        $file = Get-Item $dbPath
        Write-Host "Status: EXISTS" -ForegroundColor Green
        Write-Host "Size: $([math]::Round($file.Length/1KB, 2)) KB"
        Write-Host "Last Modified: $($file.LastWriteTime)"
    } else {
        Write-Host "Status: NOT CREATED YET" -ForegroundColor Yellow
        Write-Host "Wait 1-5 minutes after first connection"
    }
} else {
    Write-Host "CrossNetShareClient is not running" -ForegroundColor Red
}
'@

$script | Out-File -FilePath "find_index.ps1" -Encoding UTF8
.\find_index.ps1
```

## 迁移现有索引（可选）

如果你想保留旧的索引数据：

```powershell
# 1. 找到旧索引
$oldPath = "$env:LOCALAPPDATA\CrossNetShareClient\content_index.db"

# 2. 找到新位置
$proc = Get-Process CrossNetShareClient -ErrorAction SilentlyContinue
if ($proc) {
    $newDir = Split-Path $proc.Path
    $newPath = Join-Path $newDir "content_index.db"
    
    # 3. 停止客户端
    Write-Host "Please STOP the client first, then press Enter..."
    Read-Host
    
    # 4. 复制索引文件
    if (Test-Path $oldPath) {
        Copy-Item $oldPath $newPath -Force
        Write-Host "Index file copied successfully"
        Write-Host "From: $oldPath"
        Write-Host "To: $newPath"
    } else {
        Write-Host "Old index file not found"
    }
}
```

## 性能影响

**无性能影响**。索引文件的读写速度与位置无关（都是本地磁盘）。

## 回滚方法

如果需要恢复到使用 AppData 目录：

```cpp
// 在 client/ui/main_window.cpp 中改回：
QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
QDir().mkpath(appDataPath);
QString dbPath = appDataPath + "/content_index.db";
```

## 更新日志

- **v2.0.1**：索引文件保存位置从 AppData 改为程序目录
- **v2.0.0**：索引文件保存在 AppData 目录

## FAQ

### Q1: 为什么我的程序目录没有写权限？
**A**: 如果安装在 `Program Files` 等受保护目录：
- 方案 1：以管理员身份运行客户端（右键 → 以管理员身份运行）
- 方案 2：安装到用户目录（如 `C:\Users\<用户名>\CrossNetShare`）
- 方案 3：将程序安装到非受保护位置（如 `D:\Apps\CrossNetShare`）

### Q2: 升级后旧索引会丢失吗？
**A**: 不会丢失，但需要重新索引：
- 旧索引仍在 AppData 目录
- 新版本会在程序目录创建新索引
- 如果需要，可以手动复制旧索引到新位置（参见"迁移现有索引"）

### Q3: 可以自定义索引文件位置吗？
**A**: 目前不支持配置，但可以通过修改代码实现：
```cpp
// 示例：保存到共享路径下的 .index 子目录
QString dbPath = sharePath + "/.index/content_index.db";
QDir().mkpath(sharePath + "/.index");
```

### Q4: 索引文件可以删除吗？
**A**: 可以。删除后：
1. 停止客户端
2. 删除 `content_index.db`
3. 重启客户端
4. 自动重新索引所有文件（3-10 分钟）

### Q5: 为什么选择程序目录而不是用户文档目录？
**A**: 
- 程序目录：索引和程序在一起，便于管理和备份
- 用户文档目录：更传统但位置不直观
- AppData 目录：最隐藏，不适合用户查看

根据用户反馈，程序目录最直观。

## 相关文件

- 修改的代码：`client/ui/main_window.cpp:709-713`
- 查找脚本：`find_index.ps1`（自动生成）
- 检查脚本：`check_index_simple.ps1`（需要更新路径）

## 总结

这个修改让索引文件位置更加直观和易于管理，特别是对于需要频繁查看或调试索引的开发者和高级用户。

**新位置**：`<客户端目录>/content_index.db` ✅
