# Aspose.Words 安装说明

CrossNetShare 使用破解版 Aspose.Words for Python 作为 Word 文档转 PDF 引擎。

## 优势

相比 Microsoft Word COM 方式：
- ✅ **更稳定** - 不依赖 Word 进程，无需处理 COM 对象失效
- ✅ **更快速** - 转换速度更快，启动时间更短
- ✅ **更可靠** - 无需担心 Word 长时间运行后的问题
- ✅ **跨平台** - 支持 Windows/Linux/macOS
- ✅ **不需要安装 Word** - 无需购买和安装 Microsoft Office
- ✅ **无许可证限制** - 破解版无需许可证文件

## 安装步骤

### 1. 安装 Python

如果尚未安装 Python，请下载并安装 Python 3.8 或更高版本：

**Windows:**
- 访问：https://www.python.org/downloads/
- 下载 Python 3.11 或 3.12（推荐）
- 安装时勾选 "Add Python to PATH"

**验证安装:**
```cmd
python --version
```

### 2. 安装破解版 Aspose.Words

使用项目提供的破解版 whl 包安装：

```cmd
# Windows x64（推荐）
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-win_amd64.whl --force-reinstall

# Windows x86
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-win32.whl --force-reinstall

# macOS Intel
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-macosx_10_14_x86_64.whl --force-reinstall

# macOS Apple Silicon
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-macosx_11_0_arm64.whl --force-reinstall

# Linux
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-manylinux1_x86_64.whl --force-reinstall
```

**验证安装:**
```cmd
python -c "import aspose.words as aw; print('Aspose.Words installed successfully')"
```

### 3. 无需许可证

破解版 Aspose.Words 已内置破解，无需任何许可证文件：
- ❌ 不需要 `1.lic` 文件
- ❌ 不需要 `aspose.lic` 文件
- ✅ 无评估水印
- ✅ 无功能限制

## Aspose.Words 包来源

- 来源：https://gitcode.com/qq_56222266/cracked_aspose
- 版本：aspose-words 25.9.0（破解版）
- 位置：`Aspose.Words/python专用whl包/`

## 故障排除

### Python 未找到

**错误:** `Python not found. Please install Python 3.8 or later.`

**解决:** 
1. 确保已安装 Python
2. 确保 Python 在系统 PATH 中
3. 重启应用程序

### Aspose.Words 模块未找到

**错误:** `ModuleNotFoundError: No module named 'aspose'`

**解决:**
```cmd
pip install Aspose.Words/python专用whl包/aspose_words-25.9.0-py3-none-win_amd64.whl --force-reinstall
```

### 转换失败

**可能原因:**
1. Word 文档已损坏
2. Python 进程超时（60秒）
3. 磁盘空间不足

## 性能对比

| 方案 | 启动时间 | 转换速度 | 稳定性 | 许可证 |
|------|---------|---------|--------|--------|
| **Aspose.Words（破解版）** | 快 (< 1s) | 快 | 优秀 | 不需要 |
| Microsoft Word | 慢 (3-5s) | 中等 | 中等 | 需要购买Office |
| LibreOffice | 慢 (2-4s) | 慢 | 良好 | 免费开源 |

## 技术细节

- 脚本位置：`server/word_to_pdf.py`
- 转换方法：调用 Python 子进程执行转换
- 超时设置：60 秒
- 缓存机制：转换结果会被缓存，避免重复转换
- 临时文件：转换过程使用系统临时目录
