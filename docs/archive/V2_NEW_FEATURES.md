# CrossNetShare v2.0 - 新功能说明

## 🆕 版本更新

从 v1.0 升级到 v2.0，新增以下重大功能：

---

## ✨ 新功能概览

### 1. 优化共享目录模式（选项A）
- ✅ 在GUI上明确标注"共享目录"概念
- ✅ 添加"立即刷新索引"按钮
- ✅ 显示共享目录的文件数量和总大小
- ✅ 区分"共享目录文件"和"上传文件"

### 2. 自动文件监控（选项C）
- ✅ 实时监控共享目录的文件变化
- ✅ 文件新增/删除/修改自动更新索引
- ✅ 无需手动刷新，自动同步
- ✅ 使用 QFileSystemWatcher 实现

### 3. Web服务器功能 🌐
- ✅ 内置HTTP服务器（基于Qt HTTP Server）
- ✅ 无需安装客户端即可访问
- ✅ 浏览器访问：http://A电脑IP:8080
- ✅ 响应式Web界面设计

### 4. 用户认证系统 🔐
- ✅ 用户名+密码登录验证
- ✅ 会话管理（Session）
- ✅ 自动登出（超时保护）
- ✅ 支持多用户配置

### 5. Web文件浏览 📂
- ✅ 左侧：文件列表树形显示
- ✅ 顶部：搜索框（按文件名关键词）
- ✅ 支持按日期筛选
- ✅ 显示文件大小、创建时间、所属客户端

### 6. 文件预览功能 👁️
- ✅ 右侧：文件预览区域
- ✅ Word文档预览（.docx, .doc）
- ✅ PDF预览
- ✅ 图片预览（jpg, png, gif）
- ✅ 文本文件预览（txt, md, log）

### 7. 打印功能 🖨️
- ✅ 直接从浏览器打印文档
- ✅ 支持打印预览
- ✅ 页面设置（纸张大小、方向）

---

## 🏗️ 技术架构变化

### 新增依赖
- Qt HTTP Server（或使用第三方库 cpp-httplib）
- 文档转换库（Word转HTML）：
  - LibreOffice（命令行转换）
  - 或 Mammoth.js（JavaScript）
  - 或 Apache POI（如果使用Java桥接）

### 新增模块
```
server/
├── web_server.h/cpp          # HTTP服务器
├── auth_manager.h/cpp        # 认证管理
├── file_watcher.h/cpp        # 文件监控
├── document_converter.h/cpp  # 文档转换（Word→HTML）
└── web/                      # Web前端资源
    ├── index.html            # 登录页面
    ├── browse.html           # 文件浏览页面
    ├── css/
    │   └── style.css         # 样式表
    └── js/
        ├── main.js           # 主逻辑
        └── preview.js        # 预览功能
```

---

## 🎯 用户使用流程

### Web访问流程

```
1. 打开浏览器 → http://192.168.1.100:8080

2. 登录页面：
   ┌─────────────────────────────┐
   │   CrossNetShare Web Portal  │
   │                             │
   │   用户名: [admin________]   │
   │   密码:   [••••••••••••]   │
   │                             │
   │       [  登录  ]            │
   └─────────────────────────────┘

3. 文件浏览页面：
   ┌───────────────────────────────────────────────┐
   │ 搜索: [report________] [搜索] [日期筛选]      │
   ├──────────────┬────────────────────────────────┤
   │ 文件列表     │ 文件预览                        │
   │              │                                │
   │ 📁 ClientB   │ [预览区域]                      │
   │  📄 report1  │                                │
   │  📄 report2  │  Report Content...             │
   │  📄 data     │                                │
   │              │                                │
   │ 📁 ClientC   │  [下载] [打印]                 │
   │  📄 file1    │                                │
   │  📄 file2    │                                │
   └──────────────┴────────────────────────────────┘

4. 点击文件 → 右侧显示预览
5. 点击"打印" → 浏览器打印对话框
6. 点击"下载" → 下载文件
```

---

## 🔧 实现方案

### 方案1：轻量级（推荐）

**Web服务器**: 使用Qt内置的 QHttpServer（Qt 6+）或第三方 cpp-httplib

**文档预览**: 
- Word文档：服务器端转换为HTML（使用LibreOffice headless）
- PDF：使用PDF.js（浏览器端渲染）
- 图片：直接显示

**优点**：
- 纯C++实现
- 依赖少
- 性能好

### 方案2：功能完整

**Web服务器**: Embedded HTTP Server（cpp-httplib）

**文档预览**:
- Word：使用 Microsoft Office Interop（Windows）
- 或者调用 LibreOffice API
- 或者使用云端转换服务

**优点**：
- 预览效果好
- 功能完整

---

## 📋 配置文件示例

### 用户配置（users.json）
```json
{
  "users": [
    {
      "username": "admin",
      "password": "hashed_password_here",
      "role": "admin"
    },
    {
      "username": "user1",
      "password": "hashed_password_here",
      "role": "user"
    }
  ]
}
```

### Web服务器配置
```json
{
  "web_server": {
    "enabled": true,
    "port": 8080,
    "max_connections": 100,
    "session_timeout": 1800
  }
}
```

---

## ⚠️ 安全考虑

### 已实现的安全措施
1. ✅ 用户认证（用户名+密码）
2. ✅ 密码哈希存储（SHA256 + Salt）
3. ✅ 会话管理（Session Token）
4. ✅ 自动登出（30分钟无操作）
5. ✅ HTTPS支持（可选，需要SSL证书）

### 建议的安全措施
1. ⚠️ 仅在内网使用
2. ⚠️ 使用强密码
3. ⚠️ 定期更换密码
4. ⚠️ 启用HTTPS（如果需要）
5. ⚠️ 限制登录尝试次数

---

## 🚀 开始实现

由于这是一个较大的功能扩展，实现将分为以下阶段：

### 阶段1：基础增强（今天完成）
- [x] 优化共享目录UI
- [x] 实现文件监控
- [ ] 基础Web服务器
- [ ] 用户认证

### 阶段2：Web界面（明天完成）
- [ ] HTML/CSS/JS前端
- [ ] 文件列表API
- [ ] 搜索功能
- [ ] 下载功能

### 阶段3：高级功能（后天完成）
- [ ] Word文档预览
- [ ] PDF预览
- [ ] 打印功能
- [ ] 性能优化

---

**预计完成时间**: 3天  
**当前进度**: 10%

现在开始实现...
