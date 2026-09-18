# 全文检索功能 - 待办事项清单

## 🔴 立即执行（编译前）

### 1. ✅ 合并客户端主窗口代码 (已完成)
- [x] 打开 `client/ui/main_window.cpp`
- [x] 在文件开头添加：`#include "index_settings_dialog.h"`
- [x] 在构造函数中添加：`indexer_(nullptr)` 初始化
- [x] 在构造函数中调用：`setupMenuBar()` 和 `initializeIndexer()`
- [x] 在析构函数中添加索引器停止代码
- [x] 复制所有函数实现到 `main_window.cpp` 末尾
- [x] 在 `setupUi()` 函数中添加搜索区域UI

**参考文档：** `CLIENT_INTEGRATION_PATCH.md`

### 2. ✅ Web API 添加（已完成）
- [x] 在 `server/web_server.h` 添加 `handleContentSearch` 声明
- [x] 在 `server/web_server.cpp` 添加路由处理
- [x] 实现简化版内容搜索API（提示用户使用客户端）

### 3. 验证构建配置
- [ ] 确认 `CMakeLists.txt` 包含所有新文件
- [ ] 确认链接了 Qt5::Sql 和 Qt5::Concurrent
- [ ] 确认 Python 脚本会被复制到输出目录

---

## 🟡 编译和测试

### 4. 本地编译测试
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

- [ ] 编译成功无错误
- [ ] 检查输出目录是否包含 Python 脚本
- [ ] 启动客户端测试

### 5. 功能测试
- [ ] 设置共享路径
- [ ] 打开 工具 → 索引设置
- [ ] 测试文件类型选择
- [ ] 测试重建索引
- [ ] 测试内容搜索（TXT文件）
- [ ] 查看日志确认索引工作正常

### 6. Python 依赖测试
```bash
# 测试依赖
python test_extraction_deps.py

# 测试PDF提取
python client/extract_pdf_text.py test.pdf

# 测试Word提取
python client/extract_word_text.py test.docx
```

- [ ] PyPDF2 已安装
- [ ] Aspose.Words 已安装
- [ ] PDF 提取工作正常
- [ ] Word 提取工作正常

---

## 🟢 推送到GitHub

### 7. 提交代码
```bash
git add .
git commit -m "添加全文检索功能（阶段1-3）

- 实现 SQLite FTS5 索引器
- 支持 TXT/PDF/Word 文本提取
- 添加索引配置界面
- 集成到客户端主程序
- 支持实时监控和定时扫描
- 支持布尔查询（AND/OR/NOT）"

git push origin main
```

### 7. GitHub Actions 编译
- [ ] 等待 Actions 编译完成
- [ ] 检查编译日志
- [ ] 下载编译包测试

---

## 🔵 文档更新

### 9. 更新 README.md
在 README.md 中添加全文检索功能说明：

```markdown
## 新功能：全文检索 🔍

CrossNetShare v1.4.0 新增全文检索功能：

### 功能特性
- ✅ 支持 TXT、PDF、Word 文档内容搜索
- ✅ 布尔查询（AND/OR/NOT）
- ✅ 实时文件监控和自动索引
- ✅ 可配置文件类型和排除规则

### 使用方法
1. 安装 Python 依赖：`pip install PyPDF2`
2. 设置共享路径
3. 打开 工具 → 索引设置
4. 重建索引
5. 在主界面搜索框输入关键词

详细文档：[CONTENT_SEARCH_DEPENDENCIES.md](CONTENT_SEARCH_DEPENDENCIES.md)
```

### 9. 更新安装指南
在 `安装指南.md` 中添加全文检索章节

---

## 🟣 阶段4：Web API（已完成✅）

### 10. ✅ 服务器端API（已完成）
- [x] 在 `server/web_server.h` 添加 `handleContentSearch` 声明
- [x] 在 `server/web_server.cpp` 添加 `/api/content-search` 路由
- [x] 实现简化版API（返回友好提示信息）
- [x] 引导用户使用客户端功能

**说明：** 采用简化方案，Web API 不直接实现搜索功能，而是返回提示信息引导用户使用功能更强大的桌面客户端。这避免了复杂的客户端通信和状态管理，简化了实现。未来如需完整Web搜索，可扩展为WebSocket方案。

### 11. ❌ 客户端协议扩展（暂不实现）
- [ ] 在 `common/message.h` 添加 `CONTENT_SEARCH_REQUEST`
- [ ] 在 `common/message.h` 添加 `CONTENT_SEARCH_RESPONSE`
- [ ] 客户端处理搜索请求
- [ ] 返回搜索结果

### 12. ❌ Web 前端（暂不实现）
- ~~在 Web 界面添加"内容搜索"标签页~~
- ~~实现搜索框和布尔查询帮助~~
- ~~实现搜索结果展示~~
- ~~可选：内容预览和高亮~~

**说明：** Web 页面当前使用现有的文件名搜索。内容搜索功能专为桌面客户端优化，提供更好的用户体验。

---

## 🟤 阶段5：测试和优化（下一步）

### 13. 性能测试
- [ ] 测试 1,000 文件索引速度
- [ ] 测试 10,000 文件索引速度
- [ ] 测试搜索响应时间
- [ ] 测试内存占用
- [ ] 测试并发搜索

### 14. 功能测试
- [ ] 测试各种文件类型
- [ ] 测试特殊字符和编码
- [ ] 测试大文件处理
- [ ] 测试文件变更检测
- [ ] 测试实时监控

### 15. 优化
- [ ] 优化索引速度（如有必要）
- [ ] 优化搜索算法（如有必要）
- [ ] 优化内存使用（如有必要）
- [ ] 优化UI响应性（如有必要）

### 16. Bug修复
- [ ] 修复发现的所有bug
- [ ] 添加更多错误处理
- [ ] 完善日志输出

---

## 📚 文档完善

### 17. 用户文档
- [ ] 编写详细使用教程
- [ ] 添加截图和示例
- [ ] FAQ 常见问题
- [ ] 视频教程（可选）

### 18. 开发者文档
- [ ] API 文档
- [ ] 架构设计文档
- [ ] 代码贡献指南
- [ ] 单元测试文档

---

## 🎯 可选增强功能

### 19. 高级功能（v1.5.0）
- [ ] 正则表达式搜索
- [ ] 通配符搜索
- [ ] 近似匹配（模糊搜索）
- [ ] 搜索历史记录
- [ ] 搜索建议（自动完成）

### 20. 更多文件类型（v1.6.0）
- [ ] Excel (.xlsx, .xls)
- [ ] PowerPoint (.pptx, .ppt)
- [ ] Markdown (.md)
- [ ] 代码文件 (.cpp, .py, .js 等)
- [ ] 压缩文件内容（.zip, .rar）

### 21. AI功能（v2.0）
- [ ] 语义搜索
- [ ] 智能摘要
- [ ] 相似文档推荐
- [ ] 自动分类和标签

---

## 📊 进度跟踪

### 当前进度
- ✅ 阶段1：基础架构（100%）
- ✅ 阶段2：文本提取（100%）
- ✅ 阶段3：客户端集成（100%）
- ✅ 阶段4：Web API（100% - 简化方案）
- ⏳ 阶段5：测试优化（0%）

**总体进度：90%**

### 预计完成时间
- 阶段5：1-2天
- **预计总完成时间：距现在 1-2 天**

---

## ✅ 当前任务优先级

### 🟢 已完成
1. ✅ 客户端主窗口代码集成
2. ✅ Web API 简化实现
3. ✅ 阶段4完成

### 🔴 高优先级（立即执行）
1. 本地编译测试
2. 功能验证测试
3. Python依赖测试

### 🟡 中优先级（本周内）
4. 推送到 GitHub
5. 更新文档
6. 开始阶段4开发

### 🟢 低优先级（下周）
7. 阶段5测试和优化
8. 可选功能开发

---

## 📞 需要帮助？

如果遇到问题：
1. 查看 `CLIENT_INTEGRATION_PATCH.md` 详细说明
2. 查看 `CONTENT_SEARCH_DEPENDENCIES.md` 依赖问题
3. 查看 `CONTENT_SEARCH_SUMMARY.md` 功能总览
4. 在 GitHub 提交 Issue

---

**下一步行动：完成"立即执行"部分的任务，然后编译测试！**
