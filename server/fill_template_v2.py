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

def replace_text_in_xml(xml_content, replacements):
    """
    在XML内容中替换文本（保留格式）
    replacements: dict { placeholder: value }
    """
    ns = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
    try:
        root = ET.fromstring(xml_content)
    except ET.ParseError:
        return xml_content

    # 遍历所有<w:t>标签
    for t_elem in root.findall('.//w:t', ns):
        if t_elem.text:
            for placeholder, value in replacements.items():
                if placeholder in t_elem.text:
                    t_elem.text = t_elem.text.replace(placeholder, value)

    return ET.tostring(root, encoding='unicode')

def fill_template(template_path, output_path, data):
    """
    填充Word模板（使用zipfile + raw XML）
    """
    # 准备替换字典
    replacements = {
        '【文件标题】': data.get('文件标题', ''),
        '【文号】': data.get('文号', ''),
        '【来文单位】': data.get('来文单位', ''),
        '【收文日期】': format_date(data.get('收文日期', '')),
        '【打印日期】': format_date(data.get('打印日期', datetime.now().strftime('%Y-%m-%d'))),
        '【经办人】': data.get('经办人', ''),
        '【拟办意见】': data.get('拟办意见', ''),
        '【批办领导】': data.get('批办领导', ''),
        '【批办意见】': data.get('批办意见', ''),
        '【办理结果】': data.get('办理结果', ''),
    }

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
                        xml_str = replace_text_in_xml(xml_str, replacements)
                        data_bytes = xml_str.encode('utf-8')
                    except Exception as e:
                        print(f"Warning: failed to replace in {item.filename}: {e}", file=sys.stderr)

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

    # 映射字段
    data = {
        '文件标题': record.get('SUMMARY', ''),
        '文号': record.get('WORD_CODE', ''),
        '来文单位': record.get('DEPARTMENT', ''),
        '收文日期': record.get('FILE_RECEIVE_DATE', ''),
        '打印日期': datetime.now().strftime('%Y-%m-%d'),
        '经办人': record.get('OPERATOR', ''),
        '拟办意见': record.get('SUGGESTION', ''),
        '批办领导': record.get('LEADER', ''),
        '批办意见': record.get('LEADER_INSTRUCTION', ''),
        '办理结果': record.get('PROCESS_RESULT', ''),
    }

    # 生成输出文件名
    receive_number = record.get('RECEIVE_NUMBER', 'unknown')
    filename = receive_number.replace('/', '-')
    docx_path = os.path.join(output_dir, f"{filename}.docx")

    # 执行填充
    try:
        fill_template(template_path, docx_path, data)
    except Exception as e:
        print(f"填充失败: {e}", file=sys.stderr)
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

