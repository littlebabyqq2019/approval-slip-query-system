# 独立 SQLite + Simple 扩展迁移指南

## 📌 目标

从 Qt SQL (QSqlDatabase) 迁移到独立的 SQLite C API，以便：
1. ✅ 启用 `load_extension` 功能
2. ✅ 加载 Simple FTS5 扩展
3. ✅ 使用 jieba 中文分词
4. ✅ 支持 7000+ 文件的高性能搜索

## 🔧 技术方案

### 架构变更

**之前（v1.5.0）**：
```
Qt Application
    ↓
QSqlDatabase (Qt SQL)
    ↓
qsqlite.dll (Qt 提供，禁用 load_extension)
    ↓
SQLite
```

**现在（v2.0.0）**：
```
Qt Application
    ↓
sqlite3 C API (直接调用)
    ↓
sqlite3.dll (官方版本，支持 load_extension)
    ↓
SQLite + Simple 扩展
```

### 关键改动

#### 1. CMakeLists.txt 修改

```cmake
# 添加 SQLite 库
if(WIN32)
    # 使用预编译的 SQLite DLL
    set(SQLITE3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/sqlite")
    set(SQLITE3_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.lib")
    
    # 链接到客户端
    target_include_directories(CrossNetShareClient PRIVATE ${SQLITE3_INCLUDE_DIR})
    target_link_libraries(CrossNetShareClient PRIVATE ${SQLITE3_LIBRARY})
    
    # 部署 DLL
    add_custom_command(TARGET CrossNetShareClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.dll"
            "$<TARGET_FILE_DIR:CrossNetShareClient>/sqlite3.dll"
        COMMENT "Copying SQLite DLL..."
    )
endif()
```

#### 2. FileIndexer 头文件重写

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <sqlite3.h>  // 使用 SQLite C API

namespace CrossNetShare {

class FileIndexer : public QObject {
    Q_OBJECT

public:
    explicit FileIndexer(QObject* parent = nullptr);
    ~FileIndexer();

    bool initialize(const QString& sharedPath, const QString& dbPath);
    QStringList search(const QString& query, const QStringList& fileTypes = QStringList());
    // ... 其他方法保持不变

private:
    sqlite3* db_;  // 不再使用 QSqlDatabase
    
    bool initializeDatabase();
    bool loadSimpleExtension();
    bool createTables();
    
    // SQLite C API 辅助函数
    bool execSQL(const QString& sql);
    QString escapeString(const QString& str);
};

} // namespace CrossNetShare
```

#### 3. 数据库初始化

```cpp
bool FileIndexer::initializeDatabase() {
    // 打开数据库
    int rc = sqlite3_open(dbPath_.toUtf8().constData(), &db_);
    if (rc != SQLITE_OK) {
        emit logMessage(QString("[FileIndexer] Failed to open database: %1")
            .arg(sqlite3_errmsg(db_)));
        return false;
    }
    
    emit logMessage("[FileIndexer] Database opened successfully");
    
    // 启用扩展加载（关键！）
    rc = sqlite3_enable_load_extension(db_, 1);
    if (rc != SQLITE_OK) {
        emit logMessage(QString("[FileIndexer] Failed to enable extensions: %1")
            .arg(sqlite3_errmsg(db_)));
        return false;
    }
    
    emit logMessage("[FileIndexer] Extension loading enabled");
    
    // 加载 Simple 扩展
    if (!loadSimpleExtension()) {
        emit logMessage("[FileIndexer] Warning: Simple extension not loaded, using unicode61");
    }
    
    // 创建表
    return createTables();
}
```

#### 4. 加载 Simple 扩展

```cpp
bool FileIndexer::loadSimpleExtension() {
    QString appDir = QCoreApplication::applicationDirPath();
    
#ifdef Q_OS_WIN
    QString simpleLib = appDir + "/simple.dll";
#elif defined(Q_OS_MAC)
    QString simpleLib = appDir + "/libsimple.dylib";
#else
    QString simpleLib = appDir + "/libsimple.so";
#endif
    
    emit logMessage(QString("[FileIndexer] Loading Simple extension: %1").arg(simpleLib));
    
    if (!QFileInfo::exists(simpleLib)) {
        emit logMessage("[FileIndexer] Simple extension not found");
        return false;
    }
    
    char* errMsg = nullptr;
    int rc = sqlite3_load_extension(
        db_,
        simpleLib.toUtf8().constData(),
        nullptr,  // entry point (使用默认)
        &errMsg
    );
    
    if (rc != SQLITE_OK) {
        QString error = errMsg ? QString::fromUtf8(errMsg) : "Unknown error";
        emit logMessage(QString("[FileIndexer] Failed to load Simple: %1").arg(error));
        sqlite3_free(errMsg);
        return false;
    }
    
    emit logMessage("[FileIndexer] Simple extension loaded successfully!");
    emit logMessage("[FileIndexer] Chinese search with jieba enabled");
    return true;
}
```

#### 5. 创建 FTS5 表

```cpp
bool FileIndexer::createTables() {
    // 创建文件元数据表
    const char* createFilesSql = R"(
        CREATE TABLE IF NOT EXISTS files (
            file_id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            file_name TEXT NOT NULL,
            file_size INTEGER,
            file_type TEXT,
            modified_time INTEGER,
            indexed_time INTEGER,
            content_hash TEXT
        )
    )";
    
    if (!execSQL(QString::fromUtf8(createFilesSql))) {
        return false;
    }
    
    // 尝试使用 Simple tokenizer
    const char* createFtsSql = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
            file_id UNINDEXED,
            file_name,
            content,
            tokenize='simple'
        )
    )";
    
    if (!execSQL(QString::fromUtf8(createFtsSql))) {
        // 降级到 unicode61
        emit logMessage("[FileIndexer] Simple tokenizer not available, using unicode61");
        
        const char* createFtsFallbackSql = R"(
            CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
                file_id UNINDEXED,
                file_name,
                content,
                tokenize='unicode61 remove_diacritics 2'
            )
        )";
        
        if (!execSQL(QString::fromUtf8(createFtsFallbackSql))) {
            return false;
        }
    } else {
        emit logMessage("[FileIndexer] Using Simple tokenizer with jieba");
    }
    
    // 创建索引
    execSQL("CREATE INDEX IF NOT EXISTS idx_file_path ON files(file_path)");
    execSQL("CREATE INDEX IF NOT EXISTS idx_file_type ON files(file_type)");
    
    emit logMessage("[FileIndexer] Tables created successfully");
    return true;
}
```

#### 6. SQL 执行辅助函数

```cpp
bool FileIndexer::execSQL(const QString& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.toUtf8().constData(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        QString error = errMsg ? QString::fromUtf8(errMsg) : "Unknown error";
        emit logMessage(QString("[FileIndexer] SQL error: %1").arg(error));
        emit logMessage(QString("[FileIndexer] SQL: %1").arg(sql));
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}
```

#### 7. 搜索功能

```cpp
QStringList FileIndexer::search(const QString& query, const QStringList& fileTypes) {
    if (query.trimmed().isEmpty()) {
        return QStringList();
    }
    
    emit logMessage(QString("[FileIndexer] Searching for: '%1'").arg(query));
    
    // 构建 FTS 查询
    QString ftsQuery = query;  // Simple 会自动分词
    
    QString sql = QString(
        "SELECT DISTINCT f.file_path, f.file_name "
        "FROM files f "
        "JOIN files_fts fts ON f.file_id = CAST(fts.file_id AS INTEGER) "
        "WHERE files_fts MATCH '%1' "
        "LIMIT 1000"
    ).arg(escapeString(ftsQuery));
    
    emit logMessage(QString("[FileIndexer] SQL: %1").arg(sql));
    
    QStringList results;
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        emit logMessage(QString("[FileIndexer] Failed to prepare statement: %1")
            .arg(sqlite3_errmsg(db_)));
        return results;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* filePath = sqlite3_column_text(stmt, 0);
        const unsigned char* fileName = sqlite3_column_text(stmt, 1);
        
        if (filePath) {
            results << QString::fromUtf8(reinterpret_cast<const char*>(filePath));
            emit logMessage(QString("[FileIndexer]   Found: %1")
                .arg(QString::fromUtf8(reinterpret_cast<const char*>(fileName))));
        }
    }
    
    sqlite3_finalize(stmt);
    
    emit logMessage(QString("[FileIndexer] Search completed: %1 results").arg(results.size()));
    return results;
}
```

## 📦 所需文件

### 1. SQLite 官方库

**下载地址**：
- https://www.sqlite.org/download.html
- 下载 "Precompiled Binaries for Windows" (sqlite-dll-win64-x64)

**需要文件**：
- `sqlite3.h` - 头文件
- `sqlite3.lib` - 导入库
- `sqlite3.dll` - 运行时库

**放置位置**：
```
project/
  third_party/
    sqlite/
      sqlite3.h
      sqlite3.lib
      sqlite3.dll
```

### 2. Simple 扩展

已有 `simple.dll`（GitHub Actions 构建）

### 3. jieba 词典

Simple 内置，无需额外文件

## 🧪 测试计划

### 功能测试

**测试 1：基本搜索**
```
索引 7 个文件
搜索 "雁塔"
预期：< 50ms，返回正确结果
```

**测试 2：大规模测试**
```
索引 7000 个文件
搜索 "雁塔"
预期：< 200ms，返回正确结果
```

**测试 3：中文分词**
```
搜索 "雁"
预期：不仅匹配单字，还匹配包含"雁塔"的文件
```

**测试 4：布尔运算符**
```
搜索 "雁 AND 塔"
预期：正确使用 FTS5 AND 语法
```

### 性能基准

| 文件数量 | 索引时间 | 搜索时间 | 内存占用 |
|----------|----------|----------|----------|
| 100 | < 10s | < 50ms | ~10MB |
| 1000 | < 2min | < 100ms | ~50MB |
| 7000 | < 15min | < 200ms | ~200MB |

## 🚀 部署更新

### GitHub Actions 修改

1. 保持 Simple DLL 构建（已有）
2. 添加 SQLite DLL 下载和打包
3. 更新版本到 v2.0.0

### 用户升级

**重要**：需要重建索引
1. 删除旧数据库：`content_index.db`
2. 启动新版本客户端
3. 自动重建索引（使用新的 Simple tokenizer）

## 📝 回滚计划

如果出现问题，可以：
1. 恢复 v1.5.0 版本
2. 使用 LIKE 查询作为临时方案
3. 问题修复后再次尝试

## ⏱️ 预计时间表

- **准备工作**：15 分钟（下载 SQLite，配置 CMake）
- **FileIndexer 重写**：1.5 小时
- **测试调试**：30 分钟
- **文档更新**：15 分钟

**总计**：约 2.5 小时

---

**开始时间**：待定  
**预计完成**：开始后 2.5 小时  
**版本号**：v2.0.0
