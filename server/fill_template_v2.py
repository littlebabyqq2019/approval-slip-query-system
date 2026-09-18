#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import zipfile
import os
import json
import re
from xml.etree import ElementTree as ET
from datetime import datetime

# Force UTF-8 on Windows
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

def format_date(date_str):
    """格式化日期字符串,去掉前导零"""
    if not date_str:
        return ''
    date_str = date_str.replace('年', '-').replace('月', '-').replace('日', '')
    parts = date_str.split('-')
    if len(parts) == 3:
        try:
            return f"{int(parts[0])}-{int(parts[1])}-{int(parts[2])}"
        except ValueError:
            return date_str
    return date_str

def get_cell_text(cell_elem, ns):
    """提取单元格内的所有文本"""
    texts = []
    for t_elem in cell_elem.findall('.//w:t', ns):
        if t_elem.text:
            texts.append(t_elem.text)
    return ''.join(texts)

def set_cell_text(cell_elem, new_text, ns):
    """设置单元格文本（清空现有<w:t>并设置第一个）"""
    if not new_text:
        return

    # 查找所有<w:t>
    t_elems = cell_elem.findall('.//w:t', ns)
    if not t_elems:
        return

    # 设置第一个<w:t>的文本
    t_elems[0].text = new_text

    # 清空其他<w:t>
    for t_elem in t_elems[1:]:
        t_elem.text = ''

def find_content_cell_after_label(row_elem, label, ns):
    """
    在行中查找标签单元格后的第一个单元格
    模拟 fill_template.py 的 find_content_cell 逻辑
    """
    cells = row_elem.findall('.//w:tc', ns)

    # 查找包含标签文本的单元格
    label_indices = []
    for i, cell in enumerate(cells):
        cell_text = get_cell_text(cell, ns).strip()
        if label in cell_text:
            label_indices.append(i)

    if not label_indices:
        return None

    # 取最后一个标签单元格的下一个
    last_label_idx = label_indices[-1]
    content_idx = last_label_idx + 1

    if content_idx < len(cells):
        return cells[content_idx]

    return None

def find_span_cell_by_first_label(row_elem, label, ns):
    """
    查找标签后的第一个空白单元格（用于跨列大单元格）
    模拟 fill_template.py 的 find_span_cell_by_first_label 逻辑
    """
    cells = row_elem.findall('.//w:tc', ns)

    # 查找包含标签的单元格
    label_indices = []
    for i, cell in enumerate(cells):
        cell_text = get_cell_text(cell, ns).strip()
        if label in cell_text:
            label_indices.append(i)

    start_idx = 1  # 默认从第二列开始
    if label_indices:
        start_idx = label_indices[-1] + 1

    # 查找第一个空白单元格
    for i in range(start_idx, len(cells)):
        cell_text = get_cell_text(cells[i], ns).strip()
        if not cell_text:
            return cells[i]

    # 如果没有空白单元格，返回最后一个
    if cells:
        return cells[-1]

    return None

def fill_table_in_document(xml_content, data):
    """
    使用 python-docx 兼容的逻辑填充表格
    通过标签文本查找单元格，而不是硬编码行列
    """
    ns = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}

    try:
        root = ET.fromstring(xml_content)
    except ET.ParseError as e:
        print(f"XML parse error: {e}", file=sys.stderr)
        return xml_content

    # 查找第一个表格
    tables = root.findall('.//w:tbl', ns)
    if not tables:
        print("Warning: No table found in document.xml", file=sys.stderr)
        return ET.tostring(root, encoding='unicode')

    table = tables[0]
    rows = table.findall('.//w:tr', ns)

    if len(rows) < 10:
        print(f"Warning: Expected 10+ rows, found {len(rows)}", file=sys.stderr)

    # 按照 fill_template.py 的映射逻辑填充
    # 行索引从0开始

    # 行0 (第1行): 来文单位
    if len(rows) > 0:
        cell = find_content_cell_after_label(rows[0], '来文单位', ns)
        if cell:
            set_cell_text(cell, data.get('DEPARTMENT', ''), ns)

    # 行1 (第2行): 来文字号、收文日期
    if len(rows) > 1:
        cell = find_content_cell_after_label(rows[1], '来文字号', ns)
        if cell:
            set_cell_text(cell, data.get('WORD_CODE', ''), ns)

        cell = find_content_cell_after_label(rows[1], '收文日期', ns)
        if cell:
            set_cell_text(cell, format_date(data.get('FILE_RECEIVE_DATE', '')), ns)

    # 行2 (第3行): 来文类型、收文途径
    if len(rows) > 2:
        cell = find_content_cell_after_label(rows[2], '来文类型', ns)
        if cell:
            set_cell_text(cell, data.get('FILE_CATEGORY', ''), ns)

        cell = find_content_cell_after_label(rows[2], '收文途径', ns)
        if cell:
            set_cell_text(cell, data.get('RECEIVE_CHANNEL', ''), ns)

    # 行3 (第4行): 紧急程度、密级、收文编号
    if len(rows) > 3:
        cell = find_content_cell_after_label(rows[3], '紧急程度', ns)
        if cell:
            set_cell_text(cell, data.get('EMERGENCY_LEVEL', ''), ns)

        cell = find_content_cell_after_label(rows[3], '密 级', ns)
        if not cell:
            cell = find_content_cell_after_label(rows[3], '密级', ns)
        if cell:
            set_cell_text(cell, data.get('SECRET_LEVEL', ''), ns)

        cell = find_content_cell_after_label(rows[3], '收文编号', ns)
        if cell:
            set_cell_text(cell, data.get('RECEIVE_NUMBER', ''), ns)

    # 行4 (第5行): 文件标题 (跨列大单元格)
    if len(rows) > 4:
        cell = find_span_cell_by_first_label(rows[4], '文件标题', ns)
        if cell:
            set_cell_text(cell, data.get('SUMMARY', ''), ns)

    # 行5 (第6行): 领导批示
    if len(rows) > 5:
        cell = find_span_cell_by_first_label(rows[5], '领导批示', ns)
        if cell:
            set_cell_text(cell, data.get('LEADER_INSTRUCTION', ''), ns)

    # 行6 (第7行): 拟办意见
    if len(rows) > 6:
        cell = find_span_cell_by_first_label(rows[6], '拟办意见', ns)
        if cell:
            set_cell_text(cell, data.get('SUGGESTION', ''), ns)

    # 行7-8: 传阅 (留空)

    # 行9 (第10行): 办理结果
    if len(rows) > 9:
        cell = find_span_cell_by_first_label(rows[9], '办理结果', ns)
        if cell:
            set_cell_text(cell, data.get('PROCESS_RESULT', ''), ns)

    return ET.tostring(root, encoding='unicode')

def fill_template(template_path, output_path, data):
    """填充Word模板（使用zipfile + raw XML）"""
    with zipfile.ZipFile(template_path, 'r') as template_zip:
        with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as output_zip:
            for item in template_zip.infolist():
                data_bytes = template_zip.read(item.filename)

                if item.filename == 'word/document.xml':
                    try:
                        xml_str = data_bytes.decode('utf-8')
                        xml_str = fill_table_in_document(xml_str, data)
                        data_bytes = xml_str.encode('utf-8')
                    except Exception as e:
                        print(f"Error filling table: {e}", file=sys.stderr)
                        import traceback
                        traceback.print_exc(file=sys.stderr)

                output_zip.writestr(item, data_bytes)

def main():
    if len(sys.argv) < 5:
        print("用法: fill_template_v2.py <模板> <record.json> <输出目录> <appDir>", file=sys.stderr)
        sys.exit(1)

    template_path = sys.argv[1]
    record_json_path = sys.argv[2]
    output_dir = sys.argv[3]

    # 读取record.json
    try:
        with open(record_json_path, 'r', encoding='utf-8-sig') as f:
            record = json.load(f)
    except Exception as e:
        print(f"读取record.json失败: {e}", file=sys.stderr)
        sys.exit(1)

    # 直接传递record（不做字段映射）
    data = record

    # 生成输出文件名
    receive_number = record.get('RECEIVE_NUMBER', 'unknown')
    filename = receive_number.replace('/', '-')
    docx_path = os.path.join(output_dir, f"{filename}.docx")

    # 执行填充
    try:
        fill_template(template_path, docx_path, data)
    except Exception as e:
        print(f"填充失败: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        sys.exit(1)

    # 返回JSON结果
    result = {
        "success": True,
        "docx_path": docx_path,
        "pdf_path": "",
        "filename": filename
    }
    print(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()


