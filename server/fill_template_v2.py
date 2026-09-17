#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import zipfile
import os
import json
from xml.etree import ElementTree as ET
from datetime import datetime

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
    if len(sys.argv) < 4:
        print("用法: fill_template_v2.py <模板路径> <输出路径> <JSON数据>", file=sys.stderr)
        sys.exit(1)

    template_path = sys.argv[1]
    output_path = sys.argv[2]
    json_data = sys.argv[3]

    # 解析JSON
    try:
        data = json.loads(json_data)
    except json.JSONDecodeError as e:
        print(f"JSON解析失败: {e}", file=sys.stderr)
        sys.exit(1)

    # 执行填充
    try:
        fill_template(template_path, output_path, data)
        print("SUCCESS")
    except Exception as e:
        print(f"填充失败: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
