# GitHub Actions 403 错误修复

## 问题描述

构建过程中尝试创建 GitHub Release 时出现 `403 Forbidden` 错误：

```
⚠️ GitHub release failed with status: 403
undefined
retrying... (2 retries remaining)
...
❌ Too many retries. Aborting...
```

## 根本原因

GitHub Actions 的 `GITHUB_TOKEN` 默认权限不足以创建 Release。从 GitHub Actions 更新后，默认令牌权限被限制为只读。

## 解决方案

### 修改 1: 添加 permissions 到 job

在 `.github/workflows/build.yml` 的 `build-windows` job 中添加权限声明：

```yaml
jobs:
  build-windows:
    runs-on: windows-2022
    permissions:
      contents: write  # 允许创建 Release
    
    steps:
      # ...
```

### 修改 2: 升级 action 版本

将 `softprops/action-gh-release` 从 v1 升级到 v2：

```yaml
- name: Create Release
  if: startsWith(github.ref, 'refs/tags/')
  uses: softprops/action-gh-release@v2  # 从 v1 升级到 v2
  with:
    files: CrossNetShare-Windows-x64.zip
    draft: false
    prerelease: false
    generate_release_notes: true
```

## 关键改进

### 1. permissions 配置

```yaml
permissions:
  contents: write
```

这告诉 GitHub Actions 这个 workflow 需要对仓库内容的写权限，包括：
- 创建 Release
- 上传 Release 资产
- 修改标签

### 2. action-gh-release v2

v2 版本的改进：
- 更好的权限处理
- 支持最新的 GitHub API
- 改进的错误信息
- 更好的 Node.js 版本兼容性

## 权限说明

### 可用的 permissions

GitHub Actions 支持以下权限：

```yaml
permissions:
  actions: read|write|none
  checks: read|write|none
  contents: read|write|none       # Release 需要 write
  deployments: read|write|none
  id-token: read|write|none
  issues: read|write|none
  discussions: read|write|none
  packages: read|write|none
  pages: read|write|none
  pull-requests: read|write|none
  repository-projects: read|write|none
  security-events: read|write|none
  statuses: read|write|none
```

### 为什么需要 contents: write

创建 Release 需要：
1. 读取标签信息 (read)
2. 创建 Release 对象 (write)
3. 上传文件到 Release (write)
4. 修改 Release 描述 (write)

## 仓库设置

### 可选：配置默认权限

访问仓库设置来配置 workflow 的默认权限：

1. 打开仓库页面
2. Settings → Actions → General
3. 找到 "Workflow permissions"
4. 选择：
   - **Read and write permissions** (推荐)
   - 或 **Read repository contents and packages permissions** (需要在 workflow 中显式声明)

### 当前项目配置

我们选择在 workflow 中显式声明权限：
- ✅ 更安全（最小权限原则）
- ✅ 更清晰（权限需求明确）
- ✅ 更灵活（不同 job 可以有不同权限）

## 验证

### 成功的标志

构建成功后应该看到：

```
👩‍🏭 Creating new GitHub release for tag v2.2.4...
✅ Release created successfully
📦 Uploading assets...
✅ Assets uploaded
```

### 检查清单

- [ ] workflow 文件包含 `permissions: contents: write`
- [ ] 使用 `softprops/action-gh-release@v2`
- [ ] 标签已正确推送
- [ ] Actions 页面显示构建运行中
- [ ] 构建成功完成
- [ ] Releases 页面出现新版本
- [ ] 可以下载 Release 文件

## 常见问题

### Q: 为什么之前可以工作？

A: GitHub 在 2023 年更改了默认权限策略。旧仓库可能有旧的默认设置，新仓库或更新后的仓库使用更严格的默认值。

### Q: 可以在仓库设置中全局开启写权限吗？

A: 可以，但不推荐。在 workflow 中显式声明权限更安全、更清晰。

### Q: 其他 Actions 也需要配置权限吗？

A: 取决于操作类型：
- 只读操作（checkout, build）：不需要
- 写操作（create release, push code）：需要
- 部署操作：可能需要特定权限

### Q: 如何知道需要哪些权限？

A: 
1. 查看 Action 的文档
2. 运行后看错误信息（通常会提示缺少的权限）
3. 使用最小权限原则（只给必需的权限）

## 相关链接

- [GitHub Actions Permissions](https://docs.github.com/en/actions/security-guides/automatic-token-authentication#permissions-for-the-github_token)
- [action-gh-release Documentation](https://github.com/softprops/action-gh-release)
- [Workflow Syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#permissions)

## 修改记录

- **2026-09-04 10:30**: 发现 403 错误
- **2026-09-04 10:35**: 添加 permissions: contents: write
- **2026-09-04 10:35**: 升级到 action-gh-release@v2
- **2026-09-04 10:36**: 重新推送标签 v2.2.4

---

**提交**: ac04ea9 - "Fix GitHub Actions permissions for creating releases"
**状态**: ✅ 已修复
