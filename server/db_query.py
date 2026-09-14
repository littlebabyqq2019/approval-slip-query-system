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
    Splits each line by pipe independently — H2 Shell adjusts column widths
    mid-output, so fixed header pipe positions don't work.
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

    cols = [c.strip() for c in lines[header_idx].split('|')]
    n_cols = len(cols)

    rows = []
    for j in range(header_idx + 1, len(lines)):
        line = lines[j]
        if not line.strip():
            continue
        if line.strip().startswith("("):  # (n rows)
            continue
        if '|' not in line:
            continue
        values = [v.strip() for v in line.split('|')]
        if len(values) == n_cols:
            row = {cols[k]: values[k] for k in range(n_cols)}
            rows.append(row)
    return rows


def compute_receive_number(rows):
    """生成收文编号: {FILE_CATEGORY}-{YY}-{sequence}
    YY = 年份后两位 (from FILE_RECEIVE_DATE)
    sequence = 该类型在该年份的排序 (ordered by FILE_RECEIVE_DATE, then ID)
    Fallback: 批办单#ID if FILE_CATEGORY is empty
    """
    from collections import defaultdict

    # Group records by (category, year) and assign sequence numbers
    groups = defaultdict(list)
    for r in rows:
        cat = (r.get('FILE_CATEGORY', '') or '').strip()
        date = (r.get('FILE_RECEIVE_DATE', '') or '').strip()
        if date == 'null' or not date:
            date = (r.get('CREATE_TIME', '') or '').strip()
        year = date[:4] if len(date) >= 4 and date[:4].isdigit() else ''
        if cat and year:
            groups[(cat, year)].append(r)

    # Sort each group by FILE_RECEIVE_DATE then ID, assign sequence
    for key, group in groups.items():
        group.sort(key=lambda r: (
            r.get('FILE_RECEIVE_DATE', '') or '',
            r.get('ID', '') or ''
        ))
        for i, r in enumerate(group, 1):
            r['RECEIVE_NUMBER'] = f"{key[0]}-{key[1][2:]}-{i}"

    # Fallback for records without category or date
    for r in rows:
        if 'RECEIVE_NUMBER' not in r or not r['RECEIVE_NUMBER']:
            rid = r.get('ID', '') or ''
            r['RECEIVE_NUMBER'] = f"批办单#{rid}"
    return rows


FIELDS = [
    "ID", "ARCHIVE", "ATTACHMENT", "CATEGORY_ID", "CREATE_TIME",
    "DAILY_ID", "DEPARTMENT", "EMERGENCY_LEVEL", "FILE_CATEGORY",
    "FILE_RECEIVE_DATE", "LEADER_INSTRUCTION", "MODIFY_TIME",
    "NOTES", "ORGANIZER", "PROCESS_RESULT", "RECEIVE_CHANNEL",
    "SECRET_LEVEL", "SUGGESTION", "SUMMARY", "WORD_CODE"
]

NL_PLACEHOLDER = '\u00b6'  # ¶ pilcrow — unlikely in government document data


def sql_select(fields):
    """Build SELECT clause with REPLACE to convert newlines to placeholder.
    H2 Shell outputs multi-line values as separate lines, breaking parse_table.
    We replace CHAR(10)/CHAR(13) with a placeholder char, then restore after parsing.
    """
    parts = []
    for f in fields:
        parts.append(
            f"REPLACE(REPLACE({f}, CHAR(10), CHAR(182)), CHAR(13), CHAR(182)) AS {f}"
        )
    return ', '.join(parts)


def restore_newlines(rows):
    """Restore newlines from pilcrow placeholder after parse_table."""
    for r in rows:
        for k, v in r.items():
            if v and NL_PLACEHOLDER in v:
                r[k] = v.replace(NL_PLACEHOLDER, '\n')
    return rows


def cmd_list(db_path):
    sql = f"SELECT {sql_select(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    rows = restore_newlines(rows)
    rows = compute_receive_number(rows)
    rows.sort(key=lambda r: r.get('RECEIVE_NUMBER', ''), reverse=True)
    print(json.dumps({"success": True, "records": rows}, ensure_ascii=False, indent=2))


def cmd_get(db_path, record_id):
    try:
        id_clause = str(int(record_id))
    except (ValueError, TypeError):
        id_clause = f"'{record_id.replace(chr(39), chr(39)+chr(39))}'"
    sql = f"SELECT {sql_select(FIELDS)} FROM FILE_DOCUMENT WHERE ID = {id_clause};"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    rows = restore_newlines(rows)
    all_rows = parse_table(run_h2(db_path, f"SELECT {sql_select(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"))
    all_rows = restore_newlines(all_rows)
    all_rows = compute_receive_number(all_rows)
    for r in all_rows:
        if str(r.get('ID')) == str(record_id):
            print(json.dumps({"success": True, "record": r}, ensure_ascii=False, indent=2))
            return
    if rows:
        print(json.dumps({"success": True, "record": rows[0]}, ensure_ascii=False, indent=2))
    else:
        print(json.dumps({"success": False, "error": "record not found"}, ensure_ascii=False))


def cmd_search(db_path, keyword):
    sql = f"SELECT {sql_select(FIELDS)} FROM FILE_DOCUMENT ORDER BY ID;"
    out = run_h2(db_path, sql)
    rows = parse_table(out)
    rows = restore_newlines(rows)
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
