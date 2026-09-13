#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
H2 数据库 FILE_DOCUMENT 读取脚本
用法:
  python db_query.py <db_path_without_ext> list           # 输出所有记录 JSON (含收文编号)
  python db_query.py <db_path_without_ext> get <id>       # 输出单条记录 JSON
  python db_query.py <db_path_without_ext> search <kw>    # 按关键字搜索
"""
import subprocess
import os
import sys
import json
import csv
import io
import re

# Force UTF-8 stdout/stderr on Windows (default console code page is GBK/CP936)
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

H2_JAR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "h2.jar")
if not os.path.exists(H2_JAR):
    H2_JAR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "h2.jar")
    H2_JAR = os.path.normpath(H2_JAR)


def run_h2(db_path, sql):
    url = f"jdbc:h2:{db_path};IFEXISTS=TRUE;ACCESS_MODE_DATA=r"
    cmd = [
        "java",
        "-Dfile.encoding=UTF-8",
        "-Dstdout.encoding=UTF-8",
        "-Dsun.stdout.encoding=UTF-8",
        "-cp", H2_JAR, "org.h2.tools.Shell",
        "-url", url,
        "-user", "sa",
        "-password", "",
        "-sql", sql
    ]
    env = os.environ.copy()
    env['JAVA_TOOL_OPTIONS'] = '-Dfile.encoding=UTF-8'
    result = subprocess.run(cmd, capture_output=True, cwd=os.path.dirname(H2_JAR), env=env)
    raw = result.stdout
    # Try UTF-8 first; if replacement chars appear, fall back to GBK (Windows Chinese default)
    text = raw.decode('utf-8', errors='replace')
    if '\ufffd' in text:
        gbk_text = raw.decode('gbk', errors='replace')
        if '\ufffd' not in gbk_text:
            return gbk_text
    return text


def parse_table(output):
    """Parse H2 Shell table output into list of dicts.
    H2 Shell format:
      COL1 | COL2 | COL3
      val1 | val2 | val3
      (N rows, X ms)
    No separator line, so we detect header by first line containing '|'
    and data rows until '(N rows' line.
    """
    lines = output.strip().split("\n")
    if not lines:
        return []

    # Find the header line (first line containing '|')
    header_idx = None
    for i, line in enumerate(lines):
        if '|' in line and not line.strip().startswith('('):
            header_idx = i
            break
    if header_idx is None:
        return []

    header_line = lines[header_idx]
    # Parse headers by pipe positions
    pipe_positions = [i for i, c in enumerate(header_line) if c == '|']
    cols = []
    for k in range(len(pipe_positions) + 1):
        if k == 0:
            start = 0
        else:
            start = pipe_positions[k - 1] + 1
        if k < len(pipe_positions):
            end = pipe_positions[k]
        else:
            end = len(header_line)
        cols.append(header_line[start:end].strip())

    rows = []
    for j in range(header_idx + 1, len(lines)):
        line = lines[j]
        if not line.strip():
            continue
        if line.strip().startswith("("):  # (n rows)
            continue
        if '|' not in line:
            continue
        # Split by pipe positions from header
        values = []
        for k in range(len(pipe_positions) + 1):
            if k == 0:
                start = 0
            else:
                start = pipe_positions[k - 1] + 1
            if k < len(pipe_positions):
                end = pipe_positions[k]
            else:
                end = len(line)
            values.append(line[start:end].strip())
        if len(values) == len(cols):
            row = {cols[k]: values[k] for k in range(len(cols))}
            rows.append(row)
    return rows


def compute_receive_number(rows):
    """按 来文类型FILE_CATEGORY + 年份后两位 + 同年同类序号 生成 收文编号 如 A-26-1"""
    # 先按 FILE_RECEIVE_DATE 升序排序编号
    def sort_key(r):
        date_str = r.get('FILE_RECEIVE_DATE', '') or ''
        return (date_str, int(r.get('ID', '0') or '0'))

    # 按 类型+年份 分组编号
    groups = {}
    sorted_rows = sorted(rows, key=sort_key)
    for r in sorted_rows:
        cat = r.get('FILE_CATEGORY', '') or 'UNKNOWN'
        date_str = r.get('FILE_RECEIVE_DATE', '') or ''
        year_suffix = date_str[2:4] if len(date_str) >= 4 else '00'
        key = (cat, year_suffix)
        if key not in groups:
            groups[key] = 1
        else:
            groups[key] += 1
        seq = groups[key]
        r['RECEIVE_NUMBER'] = f"{cat}-{year_suffix}-{seq}"
    return rows


FIELDS = [
    "ID", "ARCHIVE", "ATTACHMENT", "CATEGORY_ID", "CREATE_TIME",
    "DAILY_ID", "DEPARTMENT", "EMERGENCY_LEVEL", "FILE_CATEGORY",
    "FILE_RECEIVE_DATE", "LEADER_INSTRUCTION", "MODIFY_TIME",
    "NOTES", "ORGANIZER", "PROCESS_RESULT", "RECEIVE_CHANNEL",
    "SECRET_LEVEL", "SUGGESTION", "SUMMARY", "WORD_CODE"
]


def cmd_list(db_path):
    sql = f"SELECT {', '.join(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    rows = compute_receive_number(rows)
    # 按RECEIVE_NUMBER倒序排列，方便查看
    rows.sort(key=lambda r: r.get('RECEIVE_NUMBER', ''), reverse=True)
    print(json.dumps({"success": True, "records": rows}, ensure_ascii=False, indent=2))


def cmd_get(db_path, record_id):
    sql = f"SELECT {', '.join(FIELDS)} FROM FILE_DOCUMENT WHERE ID = {int(record_id)};"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    all_rows = parse_table(run_h2(db_path, f"SELECT {', '.join(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"))
    all_rows = compute_receive_number(all_rows)
    # 找到目标记录的 RECEIVE_NUMBER
    for r in all_rows:
        if str(r.get('ID')) == str(record_id):
            print(json.dumps({"success": True, "record": r}, ensure_ascii=False, indent=2))
            return
    if rows:
        print(json.dumps({"success": True, "record": rows[0]}, ensure_ascii=False, indent=2))
    else:
        print(json.dumps({"success": False, "error": "record not found"}, ensure_ascii=False))


def cmd_search(db_path, keyword):
    sql = f"SELECT {', '.join(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    rows = compute_receive_number(rows)
    kw = keyword.lower()
    filtered = []
    for r in rows:
        hay = " ".join(str(v) for v in r.values()).lower()
        if kw in hay:
            filtered.append(r)
    filtered.sort(key=lambda r: r.get('RECEIVE_NUMBER', ''), reverse=True)
    print(json.dumps({"success": True, "records": filtered}, ensure_ascii=False, indent=2))


def main():
    if len(sys.argv) < 3:
        print("Usage: db_query.py <db_path_no_ext> <list|get|search> [id|keyword]")
        sys.exit(1)
    db_path = sys.argv[1]
    action = sys.argv[2]
    if action == "list":
        cmd_list(db_path)
    elif action == "get":
        cmd_get(db_path, sys.argv[3])
    elif action == "search":
        cmd_search(db_path, sys.argv[3])
    else:
        print("Unknown action:", action)
        sys.exit(1)


if __name__ == "__main__":
    main()
