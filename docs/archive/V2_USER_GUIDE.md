# CrossNetShare v2.0 使用指南

## 🎉 版本更新

**v2.0** 在 v1.0 基础上新增了重要功能：

### 新增功能

1. ✅ **文件自动监控** - 共享目录文件变化自动刷新索引
2. ✅ **Web浏览器访问** - 无需安装客户端，浏览器即可访问
3. ✅ **用户认证** - 登录验证保护共享文件
4. ✅ **文件搜索** - 按文件名关键词快速搜索
5. ✅ **文件预览** - 在线预览Word、PDF、图片、文本
6. ✅ **打印功能** - 直接从浏览器打印文档

---

## 📋 部署步骤

### 步骤1：安装LibreOffice（仅A电脑需要）

**Word文档预览**需要LibreOffice：

1. 访问：https://www.libreoffice.org/download/
2. 下载Windows版本
3. 安装（默认选项即可）
4. 安装大小：约300MB
5. 安装时间：约5分钟

**验证安装**：
```powershell
# 打开PowerShell
C:\Program Files\LibreOffice\program\soffice.exe --version
```

### 步骤2：编译和部署

按照之前的GitHub Actions方式编译，或者本地编译：

```powershell
cd e:\dev\crossnet-share
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64"
cmake --build . --config Release
```

### 步骤3：运行服务器（A电脑）

1. 运行 `CrossNetShareServer.exe`
2. 配置：
   - **Port**: 8888（客户端连接端口）
   - **Enable Upload**: ✅ 勾选
   - **Enable Web**: ✅ 勾选
   - **Web Port**: 8080（浏览器访问端口）
3. 点击 **Start Server**
4. 查看状态显示：
   ```
   Server is running on port 8888 (Upload Enabled)
   Web: http://0.0.0.0:8080 (admin/admin123)
   ```

**防火墙配置**（重要）：
```powershell
# 管理员权限运行PowerShell
New-NetFirewallRule -DisplayName "CrossNetShare TCP" -Direction Inbound -Protocol TCP -LocalPort 8888 -Action Allow
New-NetFirewallRule -DisplayName "CrossNetShare Web" -Direction Inbound -Protocol TCP -LocalPort 8080 -Action Allow
```

### 步骤4：运行客户端（B和C电脑）

与v1.0相同：
- B电脑连接到 `192.168.1.100:8888`
- C电脑连接到 `10.28.168.100:8888`
- 注册并设置共享目录

---

## 🌐 Web浏览器访问

### 从任何电脑访问

**不需要安装客户端软件**，只需浏览器：

#### 从B电脑的网段访问
```
http://192.168.1.100:8080
```

#### 从C电脑的网段访问
```
http://10.28.168.100:8080
```

#### 从A电脑本地访问
```
http://localhost:8080
```

### 登录

**默认账户**：
- 用户名：`admin`
- 密码：`admin123`

⚠️ **首次登录后请修改密码！**

修改方法：编辑 `users.json` 文件（与服务器exe同目录）

---

## 📂 Web界面使用

### 1. 登录页面

```
┌─────────────────────────────┐
│   CrossNetShare             │
│                             │
│   用户名: [admin________]   │
│   密码:   [••••••••••••]   │
│                             │
│       [  登录  ]            │
└─────────────────────────────┘
```

### 2. 文件浏览页面

```
┌──────────────────────────────────────────────────────┐
│ CrossNetShare Web  [搜索框____] [搜索] [全部] [退出] │
├────────────────┬─────────────────────────────────────┤
│ 文件列表       │ 文件预览                             │
│ (420px)        │                                     │
│                │                                     │
│ 📄 report.docx │  [预览内容区域]                      │
│ 📄 data.xlsx   │                                     │
│ 📄 image.png   │                                     │
│                │                                     │
│                │  [下载] [打印]                       │
└────────────────┴─────────────────────────────────────┘
```

### 3. 搜索文件

**按文件名搜索**：
1. 在顶部搜索框输入关键词（如：`report`）
2. 点击 **搜索** 按钮
3. 显示匹配的文件
4. 点击 **全部** 返回所有文件

**搜索说明**：
- 不区分大小写
- 匹配文件名和路径
- 支持中文搜索

### 4. 预览文件

**支持的文件类型**：

| 类型 | 扩展名 | 说明 |
|------|--------|------|
| Word文档 | .doc, .docx, .rtf | 需要LibreOffice |
| PDF文档 | .pdf | 浏览器原生支持 |
| 图片 | .png, .jpg, .gif, .bmp | 直接显示 |
| 文本 | .txt, .log, .md, .csv, .json, .xml | 纯文本显示 |

**使用方法**：
1. 在左侧文件列表点击文件
2. 右侧自动显示预览
3. 如果不支持预览，会显示错误提示

**Word预览原理**：
- 服务器调用LibreOffice将Word转为HTML
- 浏览器显示HTML版本
- 保留大部分格式和图片

### 5. 下载文件

1. 点击左侧文件选中
2. 点击右上角 **下载** 按钮
3. 浏览器下载到本地

### 6. 打印文件

1. 预览文件
2. 点击 **打印** 按钮
3. 浏览器打印对话框
4. 选择打印机和设置
5. 打印

---

## 🔐 用户管理

### 默认用户

服务器首次运行会创建 `users.json`：

```json
{
  "sessionTimeout": 1800,
  "users": [
    {
      "username": "admin",
      "passwordHash": "...",
      "role": "admin"
    }
  ]
}
```

### 修改密码

**方法1：编辑配置文件**（推荐）

1. 停止服务器
2. 删除 `users.json`
3. 重新启动服务器（会重新创建）
4. 或者手动编辑passwordHash（需要SHA256计算）

**方法2：代码扩展**（未来版本）

在GUI中添加密码修改功能

### 添加新用户

编辑 `users.json`，添加新用户条目：

```json
{
  "username": "user1",
  "passwordHash": "计算的SHA256哈希",
  "role": "user"
}
```

**计算密码哈希**：
```python
import hashlib
password = "your_password"
hash_value = hashlib.sha256(password.encode()).hexdigest()
print(hash_value)
```

### 会话管理

- 会话超时：30分钟（无操作自动退出）
- 可在 `users.json` 修改 `sessionTimeout`（单位：秒）

---

## 🎯 使用场景

### 场景1：B电脑通过浏览器访问C的文件

```
1. B电脑打开浏览器
2. 访问 http://192.168.1.100:8080
3. 登录（admin/admin123）
4. 搜索文件
5. 在线预览Word文档
6. 下载需要的文件
```

**优点**：
- 不需要安装CrossNetShare客户端
- 可以从任何电脑访问
- 支持在线预览

### 场景2：临时访问（如会议室电脑）

```
1. 会议室电脑打开浏览器
2. 输入A电脑的IP和端口
3. 登录查看文件
4. 直接打印文档
```

### 场景3：移动设备访问

```
手机/平板浏览器也可以访问
1. 连接到同一网段的WiFi
2. 访问 http://A电脑IP:8080
3. 移动端响应式界面
4. 浏览和下载文件
```

---

## 🔧 故障排查

### 问题1：浏览器无法访问

**检查步骤**：
```powershell
# 1. 测试网络
ping 192.168.1.100

# 2. 测试端口
Test-NetConnection -ComputerName 192.168.1.100 -Port 8080

# 3. 检查防火墙
Get-NetFirewallRule | Where-Object {$_.LocalPort -eq 8080}

# 4. 查看服务器日志
# 在服务器GUI的日志区域查看错误
```

### 问题2：Word预览失败

**错误信息**：
```
LibreOffice not found on server. 
Install LibreOffice on computer A and ensure soffice is in PATH.
```

**解决方法**：
1. 确认A电脑已安装LibreOffice
2. 检查安装路径：
   ```powershell
   Test-Path "C:\Program Files\LibreOffice\program\soffice.exe"
   ```
3. 如果路径不同，代码会自动搜索常见位置
4. 重启服务器

### 问题3：登录失败

**检查**：
- 用户名：`admin`（全小写）
- 密码：`admin123`
- 检查 `users.json` 是否存在
- 查看服务器日志

### 问题4：文件列表为空

**原因**：
- 客户端未注册
- 共享目录为空
- 索引未刷新

**解决**：
1. 确认B和C客户端已连接并注册
2. 在服务器GUI点击 "Refresh Index"
3. 刷新浏览器页面

---

## ⚡ 性能说明

### 文件监控

- **自动刷新**：共享目录文件变化后自动更新索引
- **延迟**：约1-3秒
- **无需手动刷新**：新增、删除、修改文件自动同步

### Word预览性能

| 文件大小 | 转换时间 | 说明 |
|---------|---------|------|
| < 1MB | 1-3秒 | 快速 |
| 1-5MB | 3-10秒 | 中等 |
| > 5MB | 10-30秒 | 较慢 |

**优化建议**：
- 首次预览会转换，之后使用缓存
- 大文件建议直接下载
- PDF预览比Word快

### 并发访问

- 支持多用户同时访问
- 每个用户独立会话
- A电脑带宽是瓶颈

---

## 🔒 安全建议

### 当前版本安全性

- ✅ 用户认证（用户名+密码）
- ✅ 会话管理（30分钟超时）
- ✅ HttpOnly Cookie
- ⚠️ HTTP明文传输（无HTTPS）
- ⚠️ 简单的密码哈希

### 使用建议

1. **修改默认密码**
2. **仅在内网使用**
3. **不要暴露到公网**
4. **定期检查日志**
5. **使用复杂密码**

### 未来增强

- HTTPS加密传输
- 更强的密码哈希（bcrypt）
- 双因素认证
- 审计日志

---

## 📊 v1.0 vs v2.0 对比

| 功能 | v1.0 | v2.0 |
|------|------|------|
| 客户端GUI | ✅ | ✅ |
| 文件监控 | ❌ | ✅ |
| Web访问 | ❌ | ✅ |
| 用户认证 | ❌ | ✅ |
| 文件搜索 | 客户端 | 客户端+Web |
| 文件预览 | ❌ | ✅ |
| 打印功能 | ❌ | ✅ |
| 无需安装 | ❌ | ✅（浏览器） |

---

## 🎓 下一步

### 立即体验

1. ✅ 确认LibreOffice已安装
2. ✅ 启动服务器并启用Web
3. ✅ 打开浏览器访问
4. ✅ 登录并体验新功能

### 反馈和改进

如有问题或建议：
1. 查看服务器日志
2. 检查本文档的故障排查部分
3. 记录详细错误信息

---

**版本**: 2.0.0  
**更新日期**: 2026-07-16  
**新增功能**: 文件监控 + Web访问 + 文档预览

**重要提醒**：
- A电脑必须安装LibreOffice才能预览Word
- 默认账户admin/admin123请首次登录后修改
- Web端口8080需要在防火墙开放
