# CrossNetShare 快速开始指南

## 5分钟快速部署

### 前置准备

确保您的环境满足以下要求：
- Windows 系统（支持 Windows 10/11）
- 已安装 Qt5（推荐 5.15.2）
- 已安装 CMake 3.15+
- 已安装 Visual Studio 2017+ 或其他 C++17 编译器

### 步骤1：编译项目（首次使用）

```powershell
# 打开 PowerShell，进入项目目录
cd e:\dev\crossnet-share

# 创建构建目录
mkdir build
cd build

# 配置项目（替换为您的 Qt 安装路径）
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64"

# 编译（Release 模式）
cmake --build . --config Release

# 编译完成后，可执行文件位于：
# build/bin/CrossNetShareServer.exe
# build/bin/CrossNetShareClient.exe
```

### 步骤2：配置网络环境

假设您的网络配置如下：

```
A电脑（服务器）：
  - 网卡1 IP: 192.168.1.100   （与 B电脑 同网段）
  - 网卡2 IP: 10.28.168.100   （与 C电脑 同网段）

B电脑（客户端1）：
  - IP: 192.168.1.101

C电脑（客户端2）：
  - IP: 10.28.168.101
```

### 步骤3：启动服务器（A电脑）

1. 将 `build/bin/CrossNetShareServer.exe` 复制到 A电脑
2. 双击运行 `CrossNetShareServer.exe`
3. 在界面上：
   - Port 保持默认 8888
   - 勾选 "Enable Upload"
   - 点击 **Start Server**
4. 确认状态显示为 "Server is running"

**防火墙配置（重要）**：
```powershell
# 在 A电脑 上运行（以管理员身份）
New-NetFirewallRule -DisplayName "CrossNetShare Server" -Direction Inbound -Protocol TCP -LocalPort 8888 -Action Allow
```

### 步骤4：启动客户端（B电脑）

1. 将 `build/bin/CrossNetShareClient.exe` 复制到 B电脑
2. 双击运行 `CrossNetShareClient.exe`
3. **连接服务器**：
   - Server: `192.168.1.100`
   - Port: `8888`
   - 点击 **Connect**
4. **注册客户端**：
   - Client ID: `ClientB`
   - Share Path: 选择要共享的文件夹（例如：`D:\SharedFiles`）
   - 点击 **Register**
5. 等待提示 "Registration successful!"

### 步骤5：启动客户端（C电脑）

1. 将 `build/bin/CrossNetShareClient.exe` 复制到 C电脑
2. 双击运行 `CrossNetShareClient.exe`
3. **连接服务器**：
   - Server: `10.28.168.100`（注意：这里使用 A电脑 在另一个网段的IP）
   - Port: `8888`
   - 点击 **Connect**
4. **注册客户端**：
   - Client ID: `ClientC`
   - Share Path: 选择要共享的文件夹（例如：`E:\MyFiles`）
   - 点击 **Register**
5. 等待提示 "Registration successful!"

### 步骤6：测试文件共享

#### 在 B电脑 下载 C电脑 的文件

1. 在 B电脑 的客户端界面：
   - **Target Client**: 选择 `ClientC`
   - **From**: 选择开始日期（例如：2024-01-01）
   - **To**: 选择结束日期（例如：今天）
   - 点击 **Refresh**
2. 查看文件列表，确认显示了 C电脑 共享的文件
3. **下载文件**：
   - **Save to**: 选择保存目录（例如：`D:\Downloads`）
   - 点击 **Download Selected**
4. 等待下载完成，查看进度条和状态提示

#### 在 C电脑 上传文件给 B电脑

1. 在 C电脑 的客户端界面：
   - 点击 **Upload File...**
   - 选择要上传的文件
   - 等待上传完成
2. 在 B电脑 上点击 **Refresh**，可以看到新上传的文件

## 验证测试

### 测试场景1：小文件批量下载

1. 在 C电脑 的共享目录中准备 100 个小文件（每个 100KB）
2. 在 B电脑 上使用日期筛选下载这些文件
3. 预期结果：约 1-2 秒完成下载

### 测试场景2：跨网段通信

1. 在 B电脑 上 ping C电脑 的IP：
   ```powershell
   ping 10.28.168.101
   ```
   预期结果：无法 ping 通（不同网段）

2. 但通过 CrossNetShare 仍然可以共享文件（通过 A电脑 中转）

### 测试场景3：上传功能

1. 在 A电脑 服务器界面取消勾选 "Enable Upload"
2. 在 B电脑 尝试上传文件
3. 预期结果：上传失败
4. 重新勾选 "Enable Upload"，再次上传
5. 预期结果：上传成功

## 故障排查

### 问题：客户端连接失败

**检查清单**：
```powershell
# 1. 检查服务器是否运行
# 在 A电脑 上检查服务器状态

# 2. 测试网络连通性
# 在 B电脑 上
ping 192.168.1.100

# 在 C电脑 上
ping 10.28.168.100

# 3. 检查端口是否被占用
# 在 A电脑 上
netstat -ano | findstr :8888

# 4. 测试端口连接
# 在 B电脑 上
Test-NetConnection -ComputerName 192.168.1.100 -Port 8888
```

### 问题：文件列表为空

**解决步骤**：
1. 确认共享目录中有文件
2. 检查日期筛选范围是否正确
3. 在服务器端点击 "Refresh Index"
4. 在客户端重新点击 "Refresh"

### 问题：下载速度慢

**优化建议**：
1. 确认 A电脑 有足够的带宽（同时连接两个网段）
2. 避免在 A电脑 上运行其他高负载任务
3. 使用有线网络而非无线
4. 如果文件很大，考虑分批下载

## 性能基准

在标准配置下（千兆网络，A电脑为中等性能PC）：

| 场景 | 性能指标 |
|------|---------|
| 100个文件（每个100KB） | 约 1-2 秒 |
| 1000个文件（每个100KB） | 约 10-15 秒 |
| 单个大文件（100MB） | 约 10-20 秒 |
| 文件索引刷新 | 约 1-3 秒 |

## 下一步

- 阅读完整的 [README.md](README.md) 了解详细功能
- 探索高级配置选项
- 根据需要自定义源代码

## 技术支持

如遇到问题：
1. 查看服务器和客户端的日志输出
2. 检查本文档的故障排查部分
3. 提交 Issue 描述问题详情

---

**版本**：1.0.0  
**最后更新**：2026-07-16
