# 全文检索功能 Python 依赖说明

## 必需的 Python 包

全文检索功能需要以下 Python 包来提取不同格式文件的文本内容：

### 1. Aspose.Words（必需 - Word文档）

**用途：** 提取 .doc 和 .docx 文件的文本内容

**安装方法：**
```bash
# 使用项目提供的破解版 whl 包
cd Aspose.Words/python专用whl包
pip install aspose_words-25.9.0-py3-none-win_amd64.whl --force-reinstall
```

**说明：**
- 这是服务器端 Word 转 PDF 功能已经使用的包
- 无需重复安装
- 支持 .doc 和 .docx 格式

---

### 2. PyPDF2（推荐 - PDF文档）

**用途：** 提取 PDF 文件的文本内容

**安装方法：**
```bash
pip install PyPDF2
```

**版本要求：** PyPDF2 >= 3.0.0

**说明：**
- 轻量级，安装快速
- 适合大多数 PDF 文件
- 对于扫描版 PDF 无法提取文本（需要 OCR）

---

### 3. pdfplumber（可选 - PDF文档增强）

**用途：** 更准确地提取 PDF 文件的文本内容

**安装方法：**
```bash
pip install pdfplumber
```

**说明：**
- 比 PyPDF2 更准确
- 对复杂 PDF 支持更好
- 可选安装，如果已安装 PyPDF2 则会优先使用 pdfplumber

---

### 4. python-docx（可选 - DOCX文档）

**用途：** 备选的 .docx 文件提取方案

**安装方法：**
```bash
pip install python-docx
```

**说明：**
- 仅支持 .docx 格式（不支持 .doc）
- 作为 Aspose.Words 的备选方案
- 可选安装，Aspose.Words 已足够

---

## 快速安装命令

### 最小安装（仅必需）
```bash
# 假设 Aspose.Words 已安装
pip install PyPDF2
```

### 推荐安装（包含增强功能）
```bash
# 假设 Aspose.Words 已安装
pip install PyPDF2 pdfplumber
```

### 完整安装（所有功能）
```bash
cd Aspose.Words/python专用whl包
pip install aspose_words-25.9.0-py3-none-win_amd64.whl --force-reinstall
pip install PyPDF2 pdfplumber python-docx
```

---

## 验证安装

创建测试脚本 `test_extraction_deps.py`:

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys

print("=" * 60)
print("检查全文检索依赖")
print("=" * 60)

# 检查 Aspose.Words
try:
    import aspose.words
    print("✓ Aspose.Words:", aspose.words.__version__)
except ImportError:
    print("✗ Aspose.Words 未安装")
    print("  安装命令: pip install aspose_words-*.whl")

# 检查 PyPDF2
try:
    import PyPDF2
    print("✓ PyPDF2:", PyPDF2.__version__)
except ImportError:
    print("✗ PyPDF2 未安装 (推荐)")
    print("  安装命令: pip install PyPDF2")

# 检查 pdfplumber
try:
    import pdfplumber
    print("✓ pdfplumber:", pdfplumber.__version__)
except ImportError:
    print("⊘ pdfplumber 未安装 (可选)")
    print("  安装命令: pip install pdfplumber")

# 检查 python-docx
try:
    import docx
    print("✓ python-docx: 已安装")
except ImportError:
    print("⊘ python-docx 未安装 (可选)")
    print("  安装命令: pip install python-docx")

print("=" * 60)

# 统计
required = 0
optional = 0

try:
    import aspose.words
    required += 1
except:
    pass

try:
    import PyPDF2
    required += 1
except:
    pass

try:
    import pdfplumber
    optional += 1
except:
    pass

try:
    import docx
    optional += 1
except:
    pass

print(f"必需包: {required}/2 已安装")
print(f"可选包: {optional}/2 已安装")

if required == 2:
    print("\n✓ 所有必需依赖已安装，全文检索功能可用！")
    sys.exit(0)
else:
    print("\n✗ 缺少必需依赖，请安装后再使用全文检索功能")
    sys.exit(1)
```

运行测试:
```bash
python test_extraction_deps.py
```

---

## 支持的文件格式

| 格式 | 依赖包 | 状态 |
|------|--------|------|
| .txt | 无（内置） | ✅ 完全支持 |
| .doc | Aspose.Words | ✅ 完全支持 |
| .docx | Aspose.Words | ✅ 完全支持 |
| .pdf | PyPDF2 或 pdfplumber | ✅ 文本型PDF支持 |
| .pdf（扫描版） | 需要 OCR | ❌ 不支持 |

---

## 故障排除

### 问题1：PyPDF2 安装失败

**错误：**
```
ERROR: Could not find a version that satisfies the requirement PyPDF2
```

**解决方案：**
```bash
# 升级 pip
python -m pip install --upgrade pip

# 重试安装
pip install PyPDF2
```

---

### 问题2：PDF 提取为空

**原因：**
- PDF 是扫描版（图片）
- PDF 使用了特殊编码
- PDF 文件损坏

**解决方案：**
1. 检查 PDF 是否为扫描版（打开后能否选中文字）
2. 尝试安装 pdfplumber: `pip install pdfplumber`
3. 对于扫描版 PDF，需要 OCR 功能（未实现）

---

### 问题3：Aspose.Words 未找到

**错误：**
```
ModuleNotFoundError: No module named 'aspose'
```

**解决方案：**
```bash
cd Aspose.Words/python专用whl包
pip install aspose_words-25.9.0-py3-none-win_amd64.whl --force-reinstall
```

---

### 问题4：Word 提取失败

**可能原因：**
- Word 文档加密
- Word 文档损坏
- Aspose.Words 许可证问题

**解决方案：**
1. 检查文档能否正常打开
2. 确认 Aspose.Words 已正确安装
3. 检查许可证文件 `1.lic` 是否存在

---

## 性能对比

| 工具 | PDF提取速度 | Word提取速度 | 准确性 |
|------|------------|-------------|--------|
| PyPDF2 | 快 (~1s/10页) | N/A | 良好 |
| pdfplumber | 中等 (~2s/10页) | N/A | 优秀 |
| Aspose.Words | N/A | 快 (~0.5s/doc) | 优秀 |

---

## 许可证说明

- **Aspose.Words**: 使用项目提供的破解版和许可证文件
- **PyPDF2**: BSD 许可证（开源免费）
- **pdfplumber**: MIT 许可证（开源免费）
- **python-docx**: MIT 许可证（开源免费）

---

## 更新日志

- **2026-08-30**: 创建文档
- 初始版本，包含 TXT/PDF/Word 支持

---

**如有问题，请查看完整的安装指南或在 GitHub 提交 Issue。**
