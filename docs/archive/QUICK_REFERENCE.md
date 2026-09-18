# CrossNetShare v2.0 快速开始

## 🆕 v2.0 新功能

**重大更新！** v2.0新增Web浏览器访问和文档预览功能：

- ✅ **文件自动监控** - 共享目录变化自动刷新
- ✅ **Web浏览器访问** - 无需安装客户端，浏览器直接访问
- ✅ **用户认证** - 登录保护（默认：admin/admin123）
- ✅ **在线搜索** - 按文件名关键词快速搜索
- ✅ **文档预览** - Word、PDF、图片、文本在线预览
- ✅ **打印功能** - 直接从浏览器打印文档

---

## 🚀 快速开始

### 5分钟体验v2.0

1. **编译或下载** - 通过GitHub Actions编译
2. **安装LibreOffice** - 仅A电脑需要（用于Word预览）
3. **启动服务器** - 勾选"Enable Web"，启动
4. **浏览器访问** - 打开 http://A电脑IP:8080
5. **登录** - admin / admin123
6. **开始使用** - 搜索、预览、下载文件

详细步骤见：[V2_USER_GUIDE.md](V2_USER_GUIDE.md)

---

## 📖 文档导航

### 快速上手
- **[QUICKSTART.md](QUICKSTART.md)** - v1.0快速部署（5分钟）
- **[V2_USER_GUIDE.md](V2_USER_GUIDE.md)** - v2.0完整使用指南 ⭐

### 编译和构建
- **[BUILD.md](BUILD.md)** - 本地编译详细步骤
- **[GITHUB_ACTIONS_GUIDE.md](GITHUB_ACTIONS_GUIDE.md)** - 云端编译指南
- **[UPLOAD_TO_GITHUB.md](UPLOAD_TO_GITHUB.md)** - 上传代码到GitHub

### 项目信息
- **[README.md](README.md)** - 完整项目说明
- **[PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)** - 架构设计
- **[SUMMARY.md](SUMMARY.md)** - v1.0项目总结

### v2.0相关
- **[V2_STAGE1_COMPLETE.md](V2_STAGE1_COMPLETE.md)** - v2.0第一阶段完成总结 ⭐
- **[V2_NEW_FEATURES.md](V2_NEW_FEATURES.md)** - 新功能详细说明
- **[V2_IMPLEMENTATION_PLAN.md](V2_IMPLEMENTATION_PLAN.md)** - 实现方案

---

## 🌐 Web访问示例

### 从浏览器访问共享文件

```
1. A电脑启动服务器（勾选Enable Web）

2. 任何电脑打开浏览器：
   - B电脑网段: http://192.168.1.100:8080
   - C电脑网段: http://10.28.168.100:8080

3. 登录（admin/admin123）

4. 搜索文件、在线预览、直接下载
```

**优势**：
- 无需安装客户端
- 手机、平板也可访问
- 支持文档预览和打印

---

## 📋 使用场景对比

| 场景 | v1.0客户端 | v2.0 Web |
|------|-----------|----------|
| 日常办公使用 | ✅ 推荐 | ✅ |
| 临时快速访问 | ❌ 需安装 | ✅ 推荐 |
| 移动设备访问 | ❌ | ✅ |
| 文档预览 | ❌ | ✅ |
| 批量下载 | ✅ | ✅ |
| 自动同步 | ✅ | ✅ |

**建议**：
- 常用电脑：安装客户端
- 临时访问：使用浏览器
- 移动设备：使用浏览器

---

## ⚙️ 功能对比

### v1.0功能（仍然可用）
- 客户端GUI应用
- TCP文件传输
- 按日期筛选
- 批量下载
- 文件上传

### v2.0新增功能
- 文件自动监控（共享目录变化自动刷新）
- Web HTTP服务器（端口8080）
- 用户认证系统（登录验证）
- 文件搜索（按关键词）
- 在线预览（Word/PDF/图片/文本）
- 打印功能

### 完整功能列表

| 功能 | v1.0 | v2.0 |
|------|------|------|
| 跨网段文件共享 | ✅ | ✅ |
| 客户端GUI | ✅ | ✅ |
| 按日期筛选 | ✅ | ✅ |
| 批量下载 | ✅ | ✅ |
| 文件上传 | ✅ | ✅ |
| **文件监控** | ❌ | ✅ |
| **Web访问** | ❌ | ✅ |
| **用户认证** | ❌ | ✅ |
| **文件搜索** | 客户端 | 客户端+Web |
| **文档预览** | ❌ | ✅ |
| **打印功能** | ❌ | ✅ |

---

## 🔧 系统要求

### A电脑（服务器）
- Windows 10/11
- 双网卡
- **LibreOffice**（用于Word预览，约300MB）

### B和C电脑（客户端）
- Windows 10/11
- 运行CrossNetShareClient.exe

### 任何电脑（Web访问）
- 现代浏览器（Chrome/Edge/Firefox）
- 无需安装软件

---

## 🎯 核心特性

### 1. 跨网段文件共享
- A电脑作为代理服务器
- B和C在不同网段通过A共享文件
- 自动文件索引和同步

### 2. 多种访问方式
- **客户端模式**：完整功能，适合日常使用
- **Web模式**：无需安装，适合临时访问

### 3. 智能文件监控
- 自动检测文件变化
- 实时刷新索引
- 无需手动操作

### 4. 丰富的预览支持
- Word文档（.doc, .docx）
- PDF文档
- 图片（jpg, png, gif）
- 文本文件

---

## 📦 安装和部署

### 方式1：GitHub Actions云编译（推荐）

1. 上传代码到GitHub
2. 等待自动编译（8-10分钟）
3. 下载exe文件
4. 直接运行

详见：[UPLOAD_TO_GITHUB.md](UPLOAD_TO_GITHUB.md)

### 方式2：本地编译

需要：
- Qt 5.15.2
- CMake 3.15+
- Visual Studio 2019+

详见：[BUILD.md](BUILD.md)

---

## 🔐 安全说明

### 当前版本
- ✅ 用户认证（登录验证）
- ✅ 会话管理（30分钟超时）
- ⚠️ HTTP明文传输（仅限内网）

### 使用建议
- **修改默认密码**（admin/admin123）
- **仅在内网使用**
- **不要暴露到公网**
- **使用强密码**

---

## 📞 技术支持

### 文档资源
- v1.0：查看 README.md 和 QUICKSTART.md
- v2.0：查看 V2_USER_GUIDE.md

### 常见问题
- 编译问题：查看 BUILD.md
- 使用问题：查看对应文档的故障排查部分
- Web访问问题：查看 V2_USER_GUIDE.md

---

## 🎉 立即开始

### 如果您是新用户

1. 阅读 [QUICKSTART.md](QUICKSTART.md) - 了解基本概念
2. 阅读 [V2_USER_GUIDE.md](V2_USER_GUIDE.md) - 了解新功能
3. 按照 [UPLOAD_TO_GITHUB.md](UPLOAD_TO_GITHUB.md) 编译
4. 开始使用！

### 如果您已有v1.0

1. 阅读 [V2_NEW_FEATURES.md](V2_NEW_FEATURES.md) - 了解更新内容
2. 阅读 [V2_USER_GUIDE.md](V2_USER_GUIDE.md) - 学习新功能
3. 更新代码并重新编译
4. **安装LibreOffice到A电脑**
5. 启用Web功能并测试

---

**版本**: 2.0.0  
**更新日期**: 2026-07-16  
**状态**: 第一阶段完成，待测试

**下一步**: [上传到GitHub并编译](UPLOAD_TO_GITHUB.md) → [安装LibreOffice](V2_USER_GUIDE.md#步骤1安装libreoffice仅a电脑需要) → [开始使用](V2_USER_GUIDE.md)
