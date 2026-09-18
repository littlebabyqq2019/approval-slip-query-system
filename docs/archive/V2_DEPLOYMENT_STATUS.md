# v2.0.0 部署状态

## ✅ 代码推送完成

**提交哈希**：ea98842  
**推送时间**：2026-09-01  
**分支**：main

---

## 📦 GitHub Actions 状态

**构建地址**：https://github.com/littlebabyqq2019/crossnet-share/actions

### 预期构建流程

1. ✅ 安装 Qt 5.15.2
2. ⏳ 下载 SQLite（官方 DLL + 头文件）
3. ⏳ 构建 Simple FTS5 扩展
4. ⏳ 配置 CMake（使用独立 SQLite）
5. ⏳ 编译项目
6. ⏳ 收集依赖（windeployqt）
7. ⏳ 打包发布（包含 sqlite3.dll + simple.dll）
8. ⏳ 上传构件

### 需要监控的关键步骤

#### 1. SQLite 下载
```powershell
# 下载 3 个文件：
- sqlite-dll-win-x64-3460100.zip (sqlite3.dll)
- sqlite-tools-win-x64-3460100.zip (sqlite3.exe)
- sqlite-amalgamation-3460100.zip (sqlite3.h)

# 生成 sqlite3.lib
lib /def:sqlite3.def /out:sqlite3.lib /machine:x64
```

**预期输出**：
```
✓ sqlite3.dll downloaded
✓ sqlite3.h downloaded
✓ sqlite3.lib generated
```

**可能问题**：
- ⚠️ lib.exe 未找到（MSVC 路径问题）
- ⚠️ sqlite3.lib 生成失败

**影响**：
- 严重：编译失败（找不到 sqlite3 符号）
- 可以降级：直接链接 sqlite3.dll（不推荐）

#### 2. Simple 扩展构建
```powershell
# 克隆 Simple 仓库
git clone https://github.com/wangfenjin/simple.git

# 构建
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**预期输出**：
```
✓ Simple extension built successfully
```

**可能位置**：
- `third_party/simple/build/Release/simple.dll`
- `third_party/simple/build/src/Release/simple.dll`
- `third_party/simple/build/libsimple/Release/simple.dll`

**可能问题**：
- ⚠️ CMake 配置失败（依赖缺失）
- ⚠️ 编译失败（C++ 版本不兼容）

**影响**：
- 中等：客户端可以运行但降级到 unicode61
- 中文搜索质量下降

#### 3. 项目编译
```cmake
# 关键：链接独立 SQLite
target_link_libraries(CrossNetShareClient 
    PRIVATE ${SQLITE3_LIBRARY}
)
```

**预期输出**：
```
Build succeeded
0 Error(s)
```

**可能问题**：
- ⚠️ 找不到 sqlite3.h（路径配置错误）
- ⚠️ 链接错误：未定义的引用 `sqlite3_open`
- ⚠️ Qt SQL 冲突（未完全移除依赖）

**影响**：
- 严重：构建失败，无法生成可执行文件

#### 4. 依赖收集
```powershell
# 复制 Simple DLL
Copy-Item simple.dll . -Force

# 复制 SQLite DLL
Copy-Item sqlite3.dll . -Force

# 复制 jieba 词典
Copy-Item dict/ . -Recurse -Force
```

**预期输出**：
```
✓ Copied simple.dll
✓ Copied sqlite3.dll
✓ Copied jieba dictionaries
```

**可能问题**：
- ⚠️ simple.dll 未找到（路径错误）
- ⚠️ sqlite3.dll 未复制

**影响**：
- 严重：运行时缺少 DLL，程序无法启动

---

## 🧪 测试计划

### 阶段 1：下载验证（构建完成后）

1. **下载构件**
   - GitHub Actions → Artifacts → CrossNetShare-Windows-x64.zip
   - 解压到测试目录

2. **验证文件**
   ```
   Client/
   ├── CrossNetShareClient.exe ✓
   ├── sqlite3.dll             ✓ (必需)
   ├── simple.dll              ✓ (可选但强烈推荐)
   ├── dict/                   ✓ (可选)
   │   ├── jieba.dict.utf8
   │   ├── hmm_model.utf8
   │   └── user.dict.utf8
   └── Qt DLLs (Qt5Core.dll 等)
   ```

3. **检查 DLL 依赖**
   ```powershell
   # 使用 Dependencies.exe 或 dumpbin
   dumpbin /dependents CrossNetShareClient.exe
   ```
   
   **预期导入**：
   - sqlite3.dll（不应该是 qsqlite.dll）
   - Qt5Core.dll
   - Qt5Widgets.dll
   - Qt5Network.dll

### 阶段 2：功能测试

#### 测试 1：启动和初始化
```
1. 启动 CrossNetShareClient.exe
2. 配置共享目录：C:/Users/asusu/Desktop/1212
3. 连接服务器

预期日志：
[FileIndexer] Opening database with SQLite C API...
[FileIndexer] Database opened successfully
[FileIndexer] Extension loading enabled
[FileIndexer] Looking for Simple extension: ...
[FileIndexer] Simple extension loaded successfully!  ← 关键
[FileIndexer] Chinese full-text search enabled with jieba tokenizer
```

**成功标志**：
- ✅ "Simple extension loaded successfully"
- ✅ "Chinese full-text search enabled"

**降级标志**：
- ⚠️ "Failed to load Simple extension"
- ⚠️ "Will use unicode61 tokenizer instead"

#### 测试 2：基本搜索（中文）
```
操作：
1. 确保已索引 7 个测试文件
2. 搜索 "雁塔"

预期结果：
- 找到 4 个文件
- 搜索时间 < 100ms
- 结果包含：
  * 投诉批办单#C-26-3113.docx
  * 小程序.txt
  * 提示词.txt
  * 新建 文本文档 (2).txt

预期日志：
[FileIndexer] Searching for: '雁塔'
[FileIndexer] Using FTS5 MATCH with Simple tokenizer  ← 关键
[FileIndexer]   Found: 投诉批办单#C-26-3113.docx
[FileIndexer]   Found: 小程序.txt
[FileIndexer]   Found: 提示词.txt
[FileIndexer]   Found: 新建 文本文档 (2).txt
[FileIndexer] Search completed: 4 results
```

**成功标志**：
- ✅ "Using FTS5 MATCH with Simple tokenizer"
- ✅ 返回 4 个结果
- ✅ 搜索时间 < 100ms

**失败标志**：
- ❌ "Using LIKE search"（降级到 v1.5.0）
- ❌ 返回 0 个结果
- ❌ 搜索时间 > 1s

#### 测试 3：布尔搜索
```
操作：
1. 搜索 "雁 AND 塔"

预期结果：
- 找到 4 个文件（同上）

预期日志：
[FileIndexer] Detected boolean operators, using boolean search
[FileIndexer] Using FTS5 MATCH for boolean search  ← 关键
```

#### 测试 4：性能测试（大规模）
```
操作：
1. 准备 1000 个文档（可以复制现有文件）
2. 重建索引
3. 搜索 "雁塔"

预期结果：
- 索引时间 < 5 分钟
- 搜索时间 < 100ms
- 内存占用 < 100MB
```

### 阶段 3：降级测试

#### 测试 5：缺少 Simple 扩展
```
操作：
1. 删除 simple.dll
2. 重启客户端
3. 搜索 "雁塔"

预期结果：
- 程序正常启动
- 日志显示 "Simple extension not found"
- 日志显示 "Will use unicode61 tokenizer instead"
- 日志显示 "Using LIKE search"
- 搜索仍返回结果（但性能差）
```

#### 测试 6：缺少 SQLite DLL
```
操作：
1. 删除 sqlite3.dll
2. 启动客户端

预期结果：
- 程序无法启动
- 错误提示："找不到 sqlite3.dll"
```

---

## ⚠️ 已知风险和应对

### 风险 1：sqlite3.lib 生成失败

**症状**：
```
LINK : fatal error LNK1181: cannot open input file 'sqlite3.lib'
```

**原因**：
- lib.exe 未找到
- sqlite3.def 不存在或格式错误

**应对**：
1. **方案 A**：修复 lib.exe 路径
   ```yaml
   - name: Setup MSVC
     uses: microsoft/setup-msbuild@v2
   
   - name: Add MSVC to PATH
     run: |
       $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
       echo "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64" | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
   ```

2. **方案 B**：使用 dlltool（MinGW）
   ```powershell
   dlltool -d sqlite3.def -l sqlite3.lib
   ```

3. **方案 C**：直接链接 DLL（最后手段）
   ```cmake
   target_link_libraries(CrossNetShareClient 
       PRIVATE "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.dll"
   )
   ```

### 风险 2：Simple 扩展编译失败

**症状**：
```
CMake Error: Could not find CMAKE_CXX_COMPILER
```

**原因**：
- C++ 编译器未正确配置
- CMake 版本不兼容

**应对**：
1. **降级接受**：继续构建，缺少 simple.dll
   - 修改 workflow 让 Simple 失败不阻止后续步骤
   - 用户可以手动下载 simple.dll

2. **使用预编译版本**：
   ```yaml
   - name: Download Simple Extension
     run: |
       # 从 GitHub Releases 下载预编译的 simple.dll
       Invoke-WebRequest -Uri "https://github.com/.../simple.dll" -OutFile simple.dll
   ```

### 风险 3：运行时 Simple 加载失败

**症状**：
```
[FileIndexer] Failed to load Simple extension: The specified module could not be found
```

**原因**：
- simple.dll 依赖的 DLL 缺失（如 vcruntime140.dll）
- 路径不正确

**应对**：
1. **检查依赖**：
   ```powershell
   dumpbin /dependents simple.dll
   ```

2. **复制 VC++ 运行时**：
   ```yaml
   - name: Copy VC++ Runtime
     run: |
       Copy-Item "C:\Windows\System32\vcruntime140.dll" build/bin/Release/
   ```

3. **接受降级**：
   - 用户看到警告但程序可用
   - 使用 unicode61 tokenizer

---

## 📊 成功标准

### 最低标准（必须满足）

- ✅ GitHub Actions 构建成功
- ✅ 客户端可以启动
- ✅ 可以连接服务器
- ✅ 可以索引文件
- ✅ 可以搜索文件（降级到 LIKE 也行）

### 目标标准（期望满足）

- ✅ Simple 扩展成功加载
- ✅ FTS5 MATCH 查询工作
- ✅ 中文搜索 "雁塔" 返回正确结果
- ✅ 搜索时间 < 100ms（7 个文件）
- ✅ 搜索时间 < 200ms（1000 个文件）

### 优秀标准（最佳情况）

- ✅ 所有依赖自动打包
- ✅ jieba 词典正确部署
- ✅ 性能符合预期（7000 文件 < 200ms）
- ✅ 无警告日志
- ✅ 用户无需手动配置

---

## 📝 后续任务

### 立即任务（v2.0.0 发布前）

1. **监控 GitHub Actions**（15 分钟）
   - 检查构建日志
   - 确认所有步骤成功
   - 下载构件

2. **功能测试**（30 分钟）
   - 测试 1-4（基本功能）
   - 确认中文搜索工作
   - 验证性能提升

3. **问题修复**（如果需要）
   - 修复构建错误
   - 修复运行时错误
   - 重新推送和测试

4. **发布 v2.0.0**
   - 创建 GitHub Release
   - 上传构件
   - 编写 Release Notes

### 中期任务（v2.1.0）

1. **实现 Web 搜索**（Task 7）
   - 服务器广播搜索请求
   - 客户端执行本地搜索
   - 聚合结果返回 Web

2. **性能优化**
   - 查询结果缓存
   - 索引增量更新优化
   - 内存使用优化

3. **用户体验**
   - 搜索结果高亮
   - 搜索历史记录
   - 智能搜索建议

### 长期任务（v3.0.0）

1. **高级搜索功能**
   - 通配符搜索
   - 正则表达式
   - 近似搜索（相似度）

2. **分布式索引**
   - 多客户端协同索引
   - 索引分片和复制
   - 负载均衡

3. **AI 增强**
   - 语义搜索
   - 智能摘要
   - 相关文档推荐

---

## 🎯 决策点

### 如果 Simple 扩展失败？

**选项 A：修复构建**
- 优点：完整功能，最佳用户体验
- 缺点：可能需要 1-2 小时调试
- 建议：如果构建错误明确且容易修复

**选项 B：接受降级**
- 优点：快速发布，基本功能可用
- 缺点：中文搜索质量差
- 建议：如果构建问题复杂，暂时降级

**选项 C：延迟发布**
- 优点：确保质量
- 缺点：用户等待
- 建议：如果问题影响核心功能

### 如果性能不达标？

**选项 A：继续优化**
- 优点：达到设计目标
- 缺点：需要更多时间
- 建议：如果有明确的优化路径

**选项 B：调整预期**
- 优点：快速发布
- 缺点：未达到原始目标
- 建议：如果性能已经明显改善（即使未达 10x）

**选项 C：分阶段发布**
- 优点：渐进式改进
- 缺点：用户体验不一致
- 建议：作为中期方案

---

**当前状态**：✅ 代码已推送，等待 GitHub Actions 构建  
**下一步**：监控构建日志，准备测试  
**预计时间**：构建 ~15 分钟，测试 ~30 分钟，总计 ~45 分钟
