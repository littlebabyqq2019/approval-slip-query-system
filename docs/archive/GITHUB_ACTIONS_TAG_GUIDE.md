# GitHub Actions 标签构建指南

## 问题说明

**问题**: 为什么在 Actions 页面没有显示标签的构建工作流，只有 main 分支的？

**原因**: GitHub Actions workflow 配置文件（`.github/workflows/build.yml`）之前只配置了监听 `push` 到分支的事件，没有配置标签触发器。

## 解决方案

### 已修改的配置

在 `.github/workflows/build.yml` 中添加了标签触发器：

```yaml
on:
  push:
    branches: [ main, master ]
    tags:
      - 'v*'  # 当推送 v 开头的标签时触发，如 v1.0.0, v2.2.4
  pull_request:
    branches: [ main, master ]
  workflow_dispatch:
```

### 自动创建 Release

添加了自动创建 GitHub Release 的步骤：

```yaml
- name: Create Release
  if: startsWith(github.ref, 'refs/tags/')
  uses: softprops/action-gh-release@v1
  with:
    files: CrossNetShare-Windows-x64.zip
    draft: false
    prerelease: false
    generate_release_notes: true
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

## 触发方式

### 方式 1: 推送标签（自动触发）

```bash
# 创建标签
git tag -a v2.2.4 -m "Release v2.2.4"

# 推送标签到 GitHub
git push origin v2.2.4
```

推送标签后，GitHub Actions 会自动：
1. 触发构建工作流
2. 编译 Windows 版本
3. 打包所有文件
4. 自动创建 GitHub Release
5. 上传构建产物到 Release 页面

### 方式 2: 手动触发

访问 GitHub Actions 页面，点击 "Run workflow" 按钮手动触发。

### 方式 3: 推送代码到 main 分支

```bash
git push origin main
```

每次推送到 main 分支也会触发构建，但不会创建 Release。

## 工作流程

### 完整的标签发布流程

```bash
# 1. 完成所有代码修改并提交
git add .
git commit -m "Your changes"
git push origin main

# 2. 创建标签
git tag -a v2.2.4 -m "Release v2.2.4 - Description"

# 3. 推送标签
git push origin v2.2.4

# 4. GitHub Actions 自动构建并创建 Release
# 5. 在 GitHub Releases 页面查看结果
```

### 查看构建状态

1. **Actions 页面**: https://github.com/littlebabyqq2019/crossnet-share/actions
   - 可以看到所有的工作流运行记录
   - 标签触发的构建会显示标签名（如 v2.2.4）
   - 分支触发的构建会显示分支名（如 main）

2. **Releases 页面**: https://github.com/littlebabyqq2019/crossnet-share/releases
   - 可以看到所有已发布的版本
   - 每个版本包含：
     - 版本号（标签名）
     - 发布说明
     - 下载链接（构建产物）

## 标签命名规范

建议使用语义化版本号：

```
v<major>.<minor>.<patch>

示例：
v1.0.0    - 第一个正式版本
v1.1.0    - 添加新功能（次版本）
v1.1.1    - Bug 修复（补丁版本）
v2.0.0    - 重大变更（主版本）
v2.2.4    - 当前版本
```

## 标签管理

### 删除本地标签

```bash
git tag -d v2.2.4
```

### 删除远程标签

```bash
git push origin :refs/tags/v2.2.4
# 或
git push origin --delete v2.2.4
```

### 列出所有标签

```bash
# 本地标签
git tag -l

# 远程标签
git ls-remote --tags origin
```

### 查看标签详情

```bash
git show v2.2.4
```

## 常见问题

### Q: 为什么推送标签后没有触发构建？

**A**: 可能的原因：
1. workflow 配置文件中没有配置标签触发器
2. 标签名不匹配配置的模式（如配置了 `v*` 但推送了 `release-1.0`）
3. workflow 文件有语法错误

**解决方法**:
- 检查 `.github/workflows/build.yml` 文件
- 确认标签名符合模式（建议使用 `v*` 格式）
- 在 Actions 页面查看是否有错误信息

### Q: Actions 页面看不到标签构建？

**A**: 
- 刷新页面
- 等待几秒钟，GitHub 需要时间处理
- 检查 workflow 是否配置了 `tags:` 触发器

### Q: 构建失败怎么办？

**A**:
1. 在 Actions 页面点击失败的工作流
2. 查看详细日志
3. 根据错误信息修复问题
4. 推送修复后的代码
5. 重新创建并推送标签

### Q: 如何重新触发标签构建？

**A**: 
```bash
# 方法 1: 删除并重新创建标签
git tag -d v2.2.4
git push origin :refs/tags/v2.2.4
git tag -a v2.2.4 -m "Release v2.2.4"
git push origin v2.2.4

# 方法 2: 使用 workflow_dispatch 手动触发
# 在 GitHub Actions 页面点击 "Run workflow"
```

## 当前配置

### v2.2.4 的配置

- ✅ 标签触发器已配置
- ✅ 自动创建 Release
- ✅ 上传构建产物
- ✅ 生成 Release Notes

### 触发条件

- 推送到 `main` 或 `master` 分支
- 推送以 `v` 开头的标签（如 `v2.2.4`）
- 创建 Pull Request
- 手动触发（workflow_dispatch）

## 验证清单

标签推送后，应该看到：

- [ ] Actions 页面出现新的工作流运行
- [ ] 工作流运行显示标签名（v2.2.4）
- [ ] 构建成功完成
- [ ] Releases 页面自动创建了新版本
- [ ] Release 中包含下载链接
- [ ] 可以下载 `CrossNetShare-Windows-x64.zip`

## 相关链接

- **仓库**: https://github.com/littlebabyqq2019/crossnet-share
- **Actions**: https://github.com/littlebabyqq2019/crossnet-share/actions
- **Releases**: https://github.com/littlebabyqq2019/crossnet-share/releases
- **标签**: https://github.com/littlebabyqq2019/crossnet-share/tags

## 修改记录

- **2026-09-04**: 添加标签触发器配置
- **2026-09-04**: 添加自动创建 Release 功能
- **2026-09-04**: 创建 v2.2.4 标签

---

**提交**: 3662c15 - "Add tag trigger to GitHub Actions workflow"
**标签**: v2.2.4
**状态**: ✅ 已配置并推送
