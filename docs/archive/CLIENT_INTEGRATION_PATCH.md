# 客户端集成补丁说明

## 需要修改的文件

### 1. client/ui/main_window.cpp

#### A. 添加头文件（文件开头）
```cpp
#include "main_window.h"
#include "index_settings_dialog.h"  // 添加这一行
#include "common/autostart.h"
```

#### B. 在构造函数中初始化索引器（在 setupUi() 之后）
```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , client_(new Client(this))
    , fileManager_(new FileManager(client_, this))
    , indexer_(nullptr)  // 添加这一行
    , trayIcon_(nullptr)
    , trayMenu_(nullptr)
{
    setupUi();
    setupMenuBar();  // 添加这一行
    setupTrayIcon();
    
    // 初始化索引器 - 添加这一块
    initializeIndexer();
    
    // 连接信号
    // ... 现有代码保持不变
}
```

#### C. 在析构函数中清理索引器
```cpp
MainWindow::~MainWindow() {
    // 停止索引器 - 添加这一块
    if (indexer_) {
        indexer_->stop();
    }
    
    // 保存配置
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/crossnet_client_config.json";
    client_->saveConfig(configPath);
    
    // ... 现有代码保持不变
}
```

#### D. 在 setupUi() 函数中添加搜索区域（在文件浏览区域之后）
在文件浏览 QGroupBox 之后添加：

```cpp
    // 内容搜索区域 - 新增整个区域
    QGroupBox* searchGroup = new QGroupBox("内容搜索");
    QHBoxLayout* searchLayout = new QHBoxLayout(searchGroup);
    
    searchEdit_ = new QLineEdit();
    searchEdit_->setPlaceholderText("输入搜索关键词（支持 AND OR NOT）...");
    searchButton_ = new QPushButton("搜索内容");
    searchResultLabel_ = new QLabel("");
    searchResultLabel_->setStyleSheet("color: blue;");
    
    searchLayout->addWidget(new QLabel("关键词:"));
    searchLayout->addWidget(searchEdit_);
    searchLayout->addWidget(searchButton_);
    searchLayout->addWidget(searchResultLabel_);
    
    connect(searchEdit_, &QLineEdit::returnPressed, this, &MainWindow::onContentSearchClicked);
    connect(searchButton_, &QPushButton::clicked, this, &MainWindow::onContentSearchClicked);
    
    browseLayout->addWidget(searchGroup);  // 添加到主布局
```

#### E. 添加新的槽函数实现（文件末尾）
```cpp
void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    // 工具菜单
    QMenu* toolsMenu = menuBar->addMenu("工具(&T)");
    
    indexSettingsAction_ = new QAction("索引设置(&I)...", this);
    indexSettingsAction_->setStatusTip("配置全文索引设置");
    connect(indexSettingsAction_, &QAction::triggered, this, &MainWindow::onIndexSettingsClicked);
    toolsMenu->addAction(indexSettingsAction_);
    
    toolsMenu->addSeparator();
    
    QAction* exitAction = new QAction("退出(&X)", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onQuitApp);
    toolsMenu->addAction(exitAction);
    
    // 帮助菜单
    QMenu* helpMenu = menuBar->addMenu("帮助(&H)");
    
    QAction* aboutAction = new QAction("关于(&A)", this);
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 CrossNetShare",
            QString("CrossNetShare v%1\n\n"
                    "跨网段文件共享工具\n\n"
                    "支持全文检索功能")
            .arg(PROJECT_VERSION));
    });
    helpMenu->addAction(aboutAction);
}

void MainWindow::initializeIndexer() {
    QString sharePath = client_->getSharePath();
    
    if (sharePath.isEmpty()) {
        onLogMessage("Share path not set, content indexing disabled");
        return;
    }
    
    // 创建索引器
    indexer_ = new FileIndexer(this);
    
    // 设置索引数据库路径
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    QString dbPath = appDataPath + "/content_index.db";
    
    if (!indexer_->initialize(sharePath, dbPath)) {
        onLogMessage("Failed to initialize content indexer");
        delete indexer_;
        indexer_ = nullptr;
        return;
    }
    
    // 设置默认配置
    IndexConfig config;
    config.enabled = true;
    config.realtimeMonitoring = true;
    config.includedExtensions = {"txt", "pdf", "doc", "docx"};
    config.excludedPatterns = {"~$*", "*.tmp", "temp/*"};
    config.maxFileSizeMB = 50;
    config.scanIntervalMinutes = 60;
    
    indexer_->setConfig(config);
    
    // 连接信号
    connect(indexer_, &FileIndexer::indexingStarted, [this]() {
        onLogMessage("Content indexing started...");
    });
    
    connect(indexer_, &FileIndexer::indexingFinished, [this]() {
        onLogMessage("Content indexing finished");
        IndexStats stats = indexer_->getStats();
        onLogMessage(QString("Indexed %1 files, total size: %2 MB")
            .arg(stats.totalFiles)
            .arg(stats.indexSizeMB));
    });
    
    connect(indexer_, &FileIndexer::indexingError, [this](const QString& error) {
        onLogMessage("Indexing error: " + error);
    });
    
    // 启动索引器
    indexer_->start();
    onLogMessage("Content indexer initialized and started");
    
    // 触发首次索引（在后台线程）
    QTimer::singleShot(2000, [this]() {
        if (indexer_) {
            onLogMessage("Starting initial content indexing (this may take a while)...");
            indexer_->rebuildIndex();
        }
    });
}

void MainWindow::onIndexSettingsClicked() {
    if (!indexer_) {
        QMessageBox::warning(this, "索引未启用",
            "内容索引功能未启用。\n\n"
            "请先设置共享路径并重启客户端。");
        return;
    }
    
    IndexSettingsDialog dialog(indexer_, this);
    dialog.exec();
}

void MainWindow::onSearchTextChanged(const QString& text) {
    // 可以在这里实现实时搜索提示
    Q_UNUSED(text);
}

void MainWindow::onContentSearchClicked() {
    QString query = searchEdit_->text().trimmed();
    
    if (query.isEmpty()) {
        searchResultLabel_->setText("");
        return;
    }
    
    if (!indexer_) {
        QMessageBox::warning(this, "索引未启用",
            "内容索引功能未启用，无法进行内容搜索。");
        return;
    }
    
    performContentSearch(query);
}

void MainWindow::performContentSearch(const QString& query) {
    searchResultLabel_->setText("搜索中...");
    searchResultLabel_->setStyleSheet("color: orange;");
    
    // 在后台线程执行搜索
    QtConcurrent::run([this, query]() {
        QStringList results = indexer_->search(query);
        
        // 回到主线程更新UI
        QMetaObject::invokeMethod(this, [this, results, query]() {
            if (results.isEmpty()) {
                searchResultLabel_->setText("未找到匹配结果");
                searchResultLabel_->setStyleSheet("color: gray;");
                onLogMessage(QString("Content search for '%1': no results").arg(query));
            } else {
                searchResultLabel_->setText(QString("找到 %1 个匹配文件").arg(results.size()));
                searchResultLabel_->setStyleSheet("color: green;");
                onLogMessage(QString("Content search for '%1': %2 results").arg(query).arg(results.size()));
                
                // 更新文件树，仅显示匹配的文件
                // TODO: 需要将路径转换为 FileMetadata 结构
                // 这里简化处理，在日志中显示结果
                onLogMessage("Search results:");
                for (const QString& path : results) {
                    onLogMessage("  - " + path);
                }
            }
        }, Qt::QueuedConnection);
    });
}
```

---

## 完整修改后的关键代码片段

### 构造函数部分
```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , client_(new Client(this))
    , fileManager_(new FileManager(client_, this))
    , indexer_(nullptr)
    , trayIcon_(nullptr)
    , trayMenu_(nullptr)
{
    setupUi();
    setupMenuBar();  // 新增
    setupTrayIcon();
    initializeIndexer();  // 新增
    
    // ... 其他代码
}
```

### 析构函数部分
```cpp
MainWindow::~MainWindow() {
    // 停止索引器
    if (indexer_) {
        indexer_->stop();
    }
    
    // ... 其他代码
}
```

---

## 编译前检查清单

- [ ] 在 main_window.cpp 开头添加 `#include "index_settings_dialog.h"`
- [ ] 在构造函数中添加 `indexer_(nullptr)` 初始化
- [ ] 调用 `setupMenuBar()` 和 `initializeIndexer()`
- [ ] 在析构函数中停止索引器
- [ ] 在 setupUi() 中添加搜索区域UI
- [ ] 添加所有新的槽函数实现
- [ ] 确保 Qt5::Concurrent 已在 CMakeLists.txt 中链接

---

## 测试步骤

1. **编译测试**
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build . --config Release
   ```

2. **功能测试**
   - 启动客户端
   - 设置共享路径
   - 打开 工具 → 索引设置
   - 点击 重建索引
   - 等待索引完成
   - 在搜索框输入关键词
   - 点击 搜索内容

3. **检查日志**
   - 查看索引启动日志
   - 查看索引完成日志
   - 查看搜索结果日志

---

## 注意事项

1. 索引器会在首次启动后2秒开始建立索引
2. 索引过程在后台异步执行，不阻塞UI
3. 搜索结果当前在日志中显示，后续可以优化到文件树
4. 需要确保Python环境已安装，否则PDF和Word提取会失败
5. 建议在README中添加Python依赖说明

---

**完成这些修改后，全文检索功能就完整集成到客户端了！**
