#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Word模板填充 + PDF生成脚本
用法:
  python fill_template.py <template.docx> <record_json_file> <output_dir> <h2_jar_path>

record JSON字段:
  所有FILE_DOCUMENT字段 + RECEIVE_NUMBER (收文编号 A-26-1)

映射到Word模板表格 (10行x12列):
  行1列3-12:   来文单位   -> DEPARTMENT
  行2列3-6:    来文字号   -> WORD_CODE
  行2列10-12:  收文日期   -> FILE_RECEIVE_DATE
  行3列3-6:    来文类型   -> FILE_CATEGORY
  行3列10-12:  收文途径   -> RECEIVE_CHANNEL
  行4列3:      紧急程度   -> EMERGENCY_LEVEL
  行4列5-6:    密 级      -> SECRET_LEVEL
  行4列10-12:  收文编号   -> RECEIVE_NUMBER
  行5列2-12:   文件标题   -> SUMMARY
  行6列2-12:   领导批示   -> LEADER_INSTRUCTION
  行7列2-12:   拟办意见   -> SUGGESTION
  行8-9:       传阅姓名/日期 -> 留空
  行10列2-12:  办理结果   -> PROCESS_RESULT
"""
import sys
import os
import json
import shutil
import subprocess
from docx import Document

# Force UTF-8 stdout/stderr on Windows (default console code page is GBK/CP936)
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass


def find_content_cell(table, row_idx, target_merged_label_left, offset_from_label=1):
    """在表格的row_idx行中，找到标签单元格后面的内容单元格。
    HACK: 由于模板中的标签行存在合并单元格，python-docx访问时，
    标签单元格（如"来文单位"）会在相邻列中重复出现。
    我们找到所有单元格text == target_merged_label_left的列索引，取最后一个，再往后+1就是内容格起点。
    """
    row = table.rows[row_idx]
    matches = []
    for i, cell in enumerate(row.cells):
        if cell.text.strip() == target_merged_label_left:
            matches.append(i)
    if not matches:
        return None
    label_last_col = matches[-1]
    content_start_col = label_last_col + 1
    if content_start_col >= len(row.cells):
        return None
    return row.cells[content_start_col]


def find_span_cell_by_first_label(table, row_idx, first_label):
    """找该行中第一个出现first_label的单元格之后的位置，
    用于文件标题/领导批示这种跨列大单元格（通常是列2开始跨到最后）。
    这里的模板中，行5的标签"文件标题"重复出现在列1和列2，所以取列2之后的第一个空白内容格。
    """
    row = table.rows[row_idx]
    # 找到 first_label 最后出现位置
    matches = [i for i, c in enumerate(row.cells) if c.text.strip() == first_label]
    if matches:
        start = matches[-1] + 1
    else:
        start = 1  # 默认从列2开始
    for i in range(start, len(row.cells)):
        cell = row.cells[i]
        if cell.text.strip() == '':
            return cell
    return row.cells[-1] if row.cells else None


def set_cell_text(cell, text, table=None, row_idx=None):
    """设置单元格文本，使用仿宋-GB2312字体，四号(14pt)。

    支持多行文本：按换行符分割为多个段落，第二段及之后添加首行缩进2字符。
    文件标题中"（注意密级管理）"显示为红色。
    """
    if cell is None:
        return
    text = text or ''
    if not cell.paragraphs:
        cell.add_paragraph()
        return

    first_para = cell.paragraphs[0]
    lines = text.split('\n') if text else ['']

    # 清空现有runs
    for run in first_para.runs:
        run.text = ''

    # 检测是否为文件标题（row_idx == 4）且包含"（注意密级管理）"
    is_title_with_secret = (row_idx == 4) and '（注意密级管理）' in lines[0]

    if is_title_with_secret:
        # 拆分文本为：前缀 + "（注意密级管理）" + 后缀
        parts = lines[0].split('（注意密级管理）')
        if len(parts) == 2:
            # 前缀（黑色）
            if parts[0]:
                run = first_para.add_run(parts[0])
                _apply_content_font(run)
            # "（注意密级管理）"（红色）
            run_red = first_para.add_run('（注意密级管理）')
            _apply_content_font(run_red)
            from docx.shared import RGBColor
            run_red.font.color.rgb = RGBColor(255, 0, 0)
            # 后缀（黑色）
            if parts[1]:
                run = first_para.add_run(parts[1])
                _apply_content_font(run)
        else:
            # 多个匹配，简单处理：全部黑色
            if first_para.runs:
                first_run = first_para.runs[0]
            else:
                first_run = first_para.add_run()
            first_run.text = lines[0]
            _apply_content_font(first_run)
    else:
        # 普通文本（黑色）
        if first_para.runs:
            first_run = first_para.runs[0]
        else:
            first_run = first_para.add_run()
        first_run.text = lines[0]
        _apply_content_font(first_run)

    # 处理多行（从第二行开始）
    while len(cell.paragraphs) > len(lines):
        p = cell.paragraphs[-1]
        p._element.getparent().remove(p._element)

    for i, line in enumerate(lines[1:], 1):
        if i < len(cell.paragraphs):
            para = cell.paragraphs[i]
            for run in para.runs:
                run.text = ''
        else:
            para = cell.add_paragraph()
        if para.runs:
            run = para.runs[0]
        else:
            run = para.add_run()
        run.text = line
        _apply_content_font(run)
        _apply_indent(para)


def _apply_indent(para):
    """Set first-line indent of 2 Chinese characters (2 * 14pt = 28pt)."""
    try:
        from docx.shared import Pt
        para.paragraph_format.first_line_indent = Pt(28)
    except Exception:
        pass


def _apply_content_font(run):
    """Apply 仿宋-GB2312 font, 四号(14pt), black, non-bold, non-italic."""
    try:
        run.font.name = '仿宋-GB2312'
        from docx.oxml.ns import qn
        rPr = run._element.get_or_add_rPr()
        rFonts = rPr.find(qn('w:rFonts'))
        if rFonts is None:
            rFonts = rPr.makeelement(qn('w:rFonts'), {})
            rPr.insert(0, rFonts)
        rFonts.set(qn('w:eastAsia'), '仿宋-GB2312')
    except Exception:
        pass
    try:
        from docx.shared import Pt
        run.font.size = Pt(14)
    except Exception:
        pass
    try:
        from docx.shared import RGBColor
        run.font.color.rgb = RGBColor(0, 0, 0)
    except Exception:
        pass
    try:
        run.font.bold = False
    except Exception:
        pass
    try:
        run.font.italic = False
    except Exception:
        pass


def fill_template(template_path, record, output_docx_path):
    """按映射填充模板。"""
    doc = Document(template_path)
    if not doc.tables:
        raise RuntimeError("模板中找不到表格")
    table = doc.tables[0]

    department = record.get('DEPARTMENT') or ''
    word_code = record.get('WORD_CODE') or ''
    receive_date = record.get('FILE_RECEIVE_DATE') or ''
    file_category = record.get('FILE_CATEGORY') or ''
    receive_channel = record.get('RECEIVE_CHANNEL') or ''
    emergency = record.get('EMERGENCY_LEVEL') or ''
    secret = record.get('SECRET_LEVEL') or ''
    receive_number = record.get('RECEIVE_NUMBER') or ''
    summary = record.get('SUMMARY') or ''
    leader = record.get('LEADER_INSTRUCTION') or ''
    suggestion = record.get('SUGGESTION') or ''
    process_result = record.get('PROCESS_RESULT') or ''

    # 行1：来文单位（列1-2重复）
    c = find_content_cell(table, 0, '来文单位')
    set_cell_text(c, department, table, 0)

    # 行2：来文字号（列1-2重复）、收文日期（列8-9重复）
    c = find_content_cell(table, 1, '来文字号')
    set_cell_text(c, word_code, table, 1)
    c = find_content_cell(table, 1, '收文日期')
    set_cell_text(c, receive_date, table, 1)

    # 行3：来文类型、收文途径
    c = find_content_cell(table, 2, '来文类型')
    set_cell_text(c, file_category, table, 2)
    c = find_content_cell(table, 2, '收文途径')
    set_cell_text(c, receive_channel, table, 2)

    # 行4：紧急程度、密级、收文编号
    c = find_content_cell(table, 3, '紧急程度')
    set_cell_text(c, emergency, table, 3)
    c = find_content_cell(table, 3, '密 级')
    if c is None:
        c = find_content_cell(table, 3, '密级')
    set_cell_text(c, secret, table, 3)
    c = find_content_cell(table, 3, '收文编号')
    set_cell_text(c, receive_number, table, 3)

    # 行5：文件标题（列1-2标签重复，后面跨列）
    c = find_span_cell_by_first_label(table, 4, '文件标题')
    set_cell_text(c, summary, table, 4)

    # 行6：领导批示
    c = find_span_cell_by_first_label(table, 5, '领导批示')
    set_cell_text(c, leader, table, 5)

    # 行7：拟办意见
    c = find_span_cell_by_first_label(table, 6, '拟办意见')
    set_cell_text(c, suggestion, table, 6)

    # 行8：传阅-姓名（留空，不处理）
    # 行9：传阅-日期（留空，不处理）

    # 行10：办理结果
    c = find_span_cell_by_first_label(table, 9, '办理结果')
    set_cell_text(c, process_result, table, 9)

    doc.save(output_docx_path)


def find_python():
    return sys.executable


def find_license(app_dir):
    candidates = [
        os.path.join(app_dir, '1.lic'),
        os.path.join(app_dir, 'aspose.words.lic'),
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return ''


def convert_docx_to_pdf(docx_path, pdf_path, app_dir):
    """使用已有的 word_to_pdf.py（Aspose）将docx转换为pdf。"""
    script = os.path.join(app_dir, 'word_to_pdf.py')
    if not os.path.exists(script):
        # 开发环境：从源码目录找
        script = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'word_to_pdf.py')
    if not os.path.exists(script):
        raise RuntimeError(f"找不到转换脚本: {script}")

    python = find_python()
    lic = find_license(app_dir)
    args = [python, script, docx_path, pdf_path]
    if lic:
        args.append(lic)

    env = os.environ.copy()
    env['JAVA_TOOL_OPTIONS'] = '-Dfile.encoding=UTF-8'
    result = subprocess.run(args, capture_output=True, text=True, env=env)
    if result.returncode != 0:
        raise RuntimeError(
            f"Aspose转换失败 exit={result.returncode}\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}"
        )
    if not os.path.exists(pdf_path):
        raise RuntimeError(f"Aspose未生成PDF文件: {pdf_path}\nstderr: {result.stderr}")


def main():
    if len(sys.argv) < 5:
        print("Usage: fill_template.py <template.docx> <record.json> <output_dir> <app_dir>")
        sys.exit(1)
    template_path = sys.argv[1]
    record_json_path = sys.argv[2]
    output_dir = sys.argv[3]
    app_dir = sys.argv[4]

    with open(record_json_path, 'r', encoding='utf-8') as f:
        record = json.load(f)

    os.makedirs(output_dir, exist_ok=True)
    base = record.get('RECEIVE_NUMBER') or record.get('ID') or 'doc'
    # 去掉非法文件名符号
    safe_base = "".join(c for c in str(base) if c not in r'\/:*?"<>|')
    output_docx = os.path.join(output_dir, safe_base + '.docx')
    output_pdf = os.path.join(output_dir, safe_base + '.pdf')

    fill_template(template_path, record, output_docx)
    convert_docx_to_pdf(output_docx, output_pdf, app_dir)

    # 输出结果 JSON
    print(json.dumps({
        "success": True,
        "docx_path": output_docx,
        "pdf_path": output_pdf,
        "filename": safe_base
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
