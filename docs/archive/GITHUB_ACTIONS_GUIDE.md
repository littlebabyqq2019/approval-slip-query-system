# 使用 GitHub Actions 云端编译指南

## 📋 概述

由于本地缺少编译环境，我已经为您配置了GitHub Actions自动编译流程。这样您可以：
- ✅ 无需安装Qt和编译器
- ✅ 在云端自动编译
- ✅ 直接下载编译好的exe文件
- ✅ 完全免费（GitHub提供）

## 🚀 使用步骤

### 步骤1：将项目上传到GitHub

#### 方法A：使用Git命令行

```powershell
# 进入项目目录
cd e:\dev\crossnet-share

# 初始化Git仓库
git init

# 添加所有文件
git add .

# 创建初始提交
git commit -m "Initial commit: CrossNetShare v1.0.0"

# 在GitHub上创建新仓库后，关联远程仓库
# 替换 YOUR_USERNAME 为您的GitHub用户名
git remote add origin https://github.com/YOUR_USERNAME/crossnet-share.git

# 推送到GitHub
git branch -M main
git push -u origin main
```

#### 方法B：使用GitHub Desktop（图形界面）

1. 下载并安装 [GitHub Desktop](https://desktop.github.com/)
2. 打开GitHub Desktop
3. File → Add Local Repository → 选择 `e:\dev\crossnet-share`
4. 点击 "Publish repository" 发布到GitHub

#### 方法C：直接在GitHub网页上创建

1. 访问 https://github.com/new
2. 创建新仓库 `crossnet-share`
3. 使用网页上传功能上传整个项目文件夹

### 步骤2：触发自动编译

上传完成后，GitHub Actions会自动开始编译。您也可以手动触发：

1. 访问您的GitHub仓库
2. 点击 **Actions** 选项卡
3. 选择 **Build CrossNetShare** 工作流
4. 点击右侧的 **Run workflow** 按钮
5. 点击绿色的 **Run workflow** 确认

### 步骤3：等待编译完成

编译过程大约需要 **5-10分钟**，您可以：
- 在 Actions 页面查看实时编译日志
- 等待编译完成（绿色✓标志）

### 步骤4：下载编译好的程序

编译成功后：

1. 在 **Actions** 页面，点击最新的工作流运行记录
2. 滚动到页面底部的 **Artifacts** 区域
3. 下载以下文件：
   - **CrossNetShare-Windows-x64.zip** - 完整打包版（推荐）
   - **CrossNetShareServer** - 仅服务器端
   - **CrossNetShareClient** - 仅客户端

4. 解压zip文件，您会看到：
```
CrossNetShare-Release/
├── Server/
│   ├── CrossNetShareServer.exe
│   ├── Qt5Core.dll
│   ├── Qt5Widgets.dll
│   ├── Qt5Network.dll
│   └── ... (其他依赖文件)
├── Client/
│   ├── CrossNetShareClient.exe
│   ├── Qt5Core.dll
│   └── ... (其他依赖文件)
├── README.md
├── QUICKSTART.md
└── BUILD.md
```

### 步骤5：部署到您的电脑

1. **A电脑（服务器）**：
   - 复制 `Server` 文件夹到A电脑
   - 双击运行 `CrossNetShareServer.exe`

2. **B电脑（客户端1）**：
   - 复制 `Client` 文件夹到B电脑
   - 双击运行 `CrossNetShareClient.exe`

3. **C电脑（客户端2）**：
   - 复制 `Client` 文件夹到C电脑
   - 双击运行 `CrossNetShareClient.exe`

## 🎯 GitHub Actions 配置说明

已创建的配置文件：`.github/workflows/build.yml`

### 触发条件
- ✅ 推送代码到 main/master 分支
- ✅ 创建 Pull Request
- ✅ 手动触发（workflow_dispatch）

### 编译流程
1. **Checkout code** - 获取源代码
2. **Install Qt** - 安装Qt 5.15.2
3. **Setup MSVC** - 配置Visual Studio编译器
4. **Configure CMake** - 配置项目
5. **Build** - 编译（Release模式）
6. **Collect Dependencies** - 收集Qt依赖DLL
7. **Package Release** - 打包发布文件
8. **Upload Artifacts** - 上传编译产物（保留30天）

## 📦 编译产物说明

### CrossNetShare-Windows-x64.zip
完整打包版，包含：
- 服务器程序 + 所有依赖
- 客户端程序 + 所有依赖
- 完整文档

**文件大小**：约 20-30 MB

### CrossNetShareServer
仅服务器端文件夹，包含：
- CrossNetShareServer.exe
- 所有必需的Qt DLL
- 平台插件

### CrossNetShareClient
仅客户端文件夹，包含：
- CrossNetShareClient.exe
- 所有必需的Qt DLL
- 平台插件

## 🔄 后续更新

如果您修改了代码，只需：

```powershell
# 提交更改
git add .
git commit -m "描述您的更改"
git push

# GitHub Actions会自动重新编译
```

## ⚠️ 注意事项

### 1. GitHub账号要求
- 需要免费的GitHub账号
- Actions分钟数限制：每月2000分钟（免费版）
- 本项目单次编译约5-10分钟

### 2. 仓库可见性
- **公开仓库**：Actions完全免费
- **私有仓库**：每月2000分钟免费额度

### 3. Artifacts保留期
- 默认保留30天
- 可以随时下载
- 过期后需要重新编译

### 4. 编译失败处理
如果编译失败：
1. 查看Actions页面的错误日志
2. 检查是否有语法错误
3. 确保所有必需文件都已上传

## 🆘 常见问题

### Q1: 如何查看编译进度？
**A**: 访问GitHub仓库 → Actions → 点击最新的工作流运行

### Q2: 编译失败怎么办？
**A**: 点击失败的步骤查看详细日志，或者提供错误信息给我

### Q3: 可以编译Debug版本吗？
**A**: 可以，修改 `.github/workflows/build.yml` 中的 `--config Release` 为 `--config Debug`

### Q4: 如何下载之前的编译版本？
**A**: 在Actions页面选择对应的工作流运行记录，下载其Artifacts

### Q5: Artifacts下载需要登录吗？
**A**: 是的，需要登录您的GitHub账号才能下载

## 🎓 下一步

完成GitHub Actions编译后：

1. ✅ 下载编译好的程序
2. ✅ 按照 `QUICKSTART.md` 部署到3台电脑
3. ✅ 测试跨网段文件共享功能

## 📞 需要帮助？

如果您在使用GitHub Actions时遇到问题：
1. 检查Actions页面的编译日志
2. 查看本文档的常见问题部分
3. 告诉我具体的错误信息，我可以帮您解决

---

**预计总时间**：
- 上传代码到GitHub：5分钟
- GitHub Actions自动编译：5-10分钟
- 下载并部署：5分钟
- **总计**：15-20分钟

**优点**：
- ✅ 无需本地安装编译环境
- ✅ 完全自动化
- ✅ 可重复编译
- ✅ 免费使用

开始上传您的代码到GitHub吧！
