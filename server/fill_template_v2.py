#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import zipfile
import os
import json
from xml.etree import ElementTree as ET
from datetime import datetime

# Force UTF-8 on Windows
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

# DOCX is a ZIP archive — we use zipfile + raw XML (no python-docx needed)

def format_date(date_str):
    """
    格式化日期字符串,去掉前导零
    输入: '2025-02-05', '2025-2-5', '2025年02月05日' 等
    输出: '2025-2-5'
    """
    if not date_str:
        return ''
    # 移除年月日
    date_str = date_str.replace('年', '-').replace('月', '-').replace('日', '')
    # 分割日期
    parts = date_str.split('-')
    if len(parts) == 3:
        # 转换为整数再转回字符串,自动去掉前导零
        try:
            return f"{int(parts[0])}-{int(parts[1])}-{int(parts[2])}"
        except ValueError:
            return date_str
    return date_str

def fill_table_cells(xml_content, data):
    """
    在document.xml中填充表格单元格（按行列位置）
    模板表格结构（10行）：
    行0: 标题行
    行1列2-11: 来文单位 (DEPARTMENT)
    行2列2-5: 来文字号 (WORD_CODE), 列9-11: 收文日期 (FILE_RECEIVE_DATE)
    行3列2-5: 来文类型 (FILE_CATEGORY), 列9-11: 收文途径 (RECEIVE_CHANNEL)
    行4列2: 紧急程度 (EMERGENCY_LEVEL), 列4-5: 密级 (SECRET_LEVEL), 列9-11: 收文编号 (RECEIVE_NUMBER)
    行5列1-11: 文件标题 (SUMMARY)
    行6列1-11: 领导批示 (LEADER_INSTRUCTION)
    行7列1-11: 拟办意见 (SUGGESTION)
    行8-9: 传阅（留空）
    行10列1-11: 办理结果 (PROCESS_RESULT)
    """
    ns = {
        'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
        'ns0': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
    }

    try:
        root = ET.fromstring(xml_content)
    except ET.ParseError as e:
        print(f"XML parse error: {e}", file=sys.stderr)
        return xml_content

    # 查找第一个表格
    tables = root.findall('.//w:tbl', ns) or root.findall('.//ns0:tbl', ns)
    if not tables:
        print("Warning: No table found in document.xml", file=sys.stderr)
        return ET.tostring(root, encoding='unicode')

    table = tables[0]
    rows = table.findall('.//w:tr', ns) or table.findall('.//ns0:tr', ns)
    if len(rows) < 11:
        print(f"Warning: Expected 11+ rows, found {len(rows)}", file=sys.stderr)

    def set_cell_text(row_idx, col_range, text):
        """设置指定行的列范围的文本"""
        if row_idx >= len(rows):
            return
        row = rows[row_idx]
        cells = row.findall('.//w:tc', ns) or row.findall('.//ns0:tc', ns)

        if isinstance(col_range, int):
            col_range = [col_range]

        for col_idx in col_range:
            if col_idx >= len(cells):
                continue
            cell = cells[col_idx]
            # 查找单元格中的第一个<w:t>或<ns0:t>标签
            t_elems = cell.findall('.//w:t', ns) or cell.findall('.//ns0:t', ns)
            if t_elems:
                # 清空其他<w:t>，只保留第一个
                for t_elem in t_elems[1:]:
                    parent = t_elem.getparent()
                    if parent is not None:
                        parent.remove(t_elem)
                t_elems[0].text = text
                break  # 只填充第一个匹配的列

    # 填充数据（对应旧版fill_template.py的表格映射）
    set_cell_text(1, range(2, 12), data.get('DEPARTMENT', ''))  # 来文单位
    set_cell_text(2, range(2, 6), data.get('WORD_CODE', ''))  # 来文字号
    set_cell_text(2, range(9, 12), format_date(data.get('FILE_RECEIVE_DATE', '')))  # 收文日期
    set_cell_text(3, range(2, 6), data.get('FILE_CATEGORY', ''))  # 来文类型
    set_cell_text(3, range(9, 12), data.get('RECEIVE_CHANNEL', ''))  # 收文途径
    set_cell_text(4, [2], data.get('EMERGENCY_LEVEL', ''))  # 紧急程度
    set_cell_text(4, range(4, 6), data.get('SECRET_LEVEL', ''))  # 密级
    set_cell_text(4, range(9, 12), data.get('RECEIVE_NUMBER', ''))  # 收文编号
    set_cell_text(5, range(1, 12), data.get('SUMMARY', ''))  # 文件标题
    set_cell_text(6, range(1, 12), data.get('LEADER_INSTRUCTION', ''))  # 领导批示
    set_cell_text(7, range(1, 12), data.get('SUGGESTION', ''))  # 拟办意见
    set_cell_text(10, range(1, 12), data.get('PROCESS_RESULT', ''))  # 办理结果

    return ET.tostring(root, encoding='unicode')

def fill_template(template_path, output_path, data):
    """
    填充Word模板（使用zipfile + raw XML）
    """
    # 打开模板DOCX (ZIP archive)
    with zipfile.ZipFile(template_path, 'r') as template_zip:
        # 创建输出DOCX
        with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as output_zip:
            for item in template_zip.infolist():
                data_bytes = template_zip.read(item.filename)

                # 只处理word/document.xml（主体内容）
                if item.filename == 'word/document.xml':
                    try:
                        xml_str = data_bytes.decode('utf-8')
                        xml_str = fill_table_cells(xml_str, data)
                        data_bytes = xml_str.encode('utf-8')
                    except Exception as e:
                        print(f"Warning: failed to fill table in {item.filename}: {e}", file=sys.stderr)
                        import traceback
                        traceback.print_exc(file=sys.stderr)

                output_zip.writestr(item, data_bytes)

def main():
    # db_manager.cpp调用: script template record.json outputDir appDir
    if len(sys.argv) < 5:
        print("用法: fill_template_v2.py <模板> <record.json> <输出目录> <appDir>", file=sys.stderr)
        sys.exit(1)

    template_path = sys.argv[1]
    record_json_path = sys.argv[2]
    output_dir = sys.argv[3]
    # app_dir = sys.argv[4]  # 未使用

    # 读取record.json
    try:
        with open(record_json_path, 'r', encoding='utf-8-sig') as f:
            record = json.load(f)
    except Exception as e:
        print(f"读取record.json失败: {e}", file=sys.stderr)
        sys.exit(1)

    # 直接传递record字段（不做映射，因为fill_table_cells按字段名匹配）
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

    # 返回JSON结果（兼容db_manager.cpp的解析）
    result = {
        "success": True,
        "docx_path": docx_path,
        "pdf_path": "",  # v2不生成PDF
        "filename": filename
    }
    print(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()


