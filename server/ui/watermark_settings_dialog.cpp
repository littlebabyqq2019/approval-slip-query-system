#include "watermark_settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QColorDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>

namespace CrossNetShare {

WatermarkSettingsDialog::WatermarkSettingsDialog(WatermarkService* service, QWidget* parent)
    : QDialog(parent)
    , watermarkService_(service)
    , currentColor_(128, 128, 128)
{
    setWindowTitle("水印设置");
    resize(700, 600);

    setupUi();
    loadSettings();
}

WatermarkSettingsDialog::~WatermarkSettingsDialog() {
}

void WatermarkSettingsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // === 全局启用水印 ===
    QGroupBox* enableGroup = new QGroupBox("功能启用", this);
    QVBoxLayout* enableLayout = new QVBoxLayout(enableGroup);

    enableWatermarkCheckBox_ = new QCheckBox("启用水印功能（关闭后所有用户Web页面隐藏水印按钮）", this);
    enableLayout->addWidget(enableWatermarkCheckBox_);

    mainLayout->addWidget(enableGroup);

    // === 关键词管理 ===
    QGroupBox* keywordsGroup = new QGroupBox("关键词管理", this);
    QVBoxLayout* keywordsLayout = new QVBoxLayout(keywordsGroup);

    keywordsTable_ = new QTableWidget(this);
    keywordsTable_->setColumnCount(3);
    keywordsTable_->setHorizontalHeaderLabels({"检测文本", "水印文本", "启用"});
    keywordsTable_->horizontalHeader()->setStretchLastSection(false);
    keywordsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    keywordsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    keywordsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    keywordsTable_->setColumnWidth(2, 60);
    keywordsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    keywordsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    keywordsLayout->addWidget(keywordsTable_);

    QHBoxLayout* keywordButtonsLayout = new QHBoxLayout();
    addButton_ = new QPushButton("添加", this);
    editButton_ = new QPushButton("编辑", this);
    deleteButton_ = new QPushButton("删除", this);
    keywordButtonsLayout->addWidget(addButton_);
    keywordButtonsLayout->addWidget(editButton_);
    keywordButtonsLayout->addWidget(deleteButton_);
    keywordButtonsLayout->addStretch();
    keywordsLayout->addLayout(keywordButtonsLayout);

    mainLayout->addWidget(keywordsGroup);

    // === 统一水印 ===
    QGroupBox* unifiedGroup = new QGroupBox("统一水印", this);
    QVBoxLayout* unifiedLayout = new QVBoxLayout(unifiedGroup);

    enableUnifiedCheckBox_ = new QCheckBox("启用统一水印（忽略关键词，所有批办单使用相同水印）", this);
    unifiedLayout->addWidget(enableUnifiedCheckBox_);

    QHBoxLayout* unifiedTextLayout = new QHBoxLayout();
    unifiedTextLayout->addWidget(new QLabel("水印文字:", this));
    unifiedTextEdit_ = new QLineEdit(this);
    unifiedTextLayout->addWidget(unifiedTextEdit_);
    unifiedLayout->addLayout(unifiedTextLayout);

    mainLayout->addWidget(unifiedGroup);

    // === 水印样式 ===
    QGroupBox* styleGroup = new QGroupBox("水印样式", this);
    QFormLayout* styleLayout = new QFormLayout(styleGroup);

    // 字号
    fontSizeSpinBox_ = new QSpinBox(this);
    fontSizeSpinBox_->setRange(8, 72);
    fontSizeSpinBox_->setSuffix(" pt");
    styleLayout->addRow("字号:", fontSizeSpinBox_);

    // 颜色
    QHBoxLayout* colorLayout = new QHBoxLayout();
    colorButton_ = new QPushButton("选择颜色", this);
    colorLayout->addWidget(colorButton_);
    colorLayout->addStretch();
    styleLayout->addRow("颜色:", colorLayout);

    // 透明度
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    opacitySlider_ = new QSlider(Qt::Horizontal, this);
    opacitySlider_->setRange(0, 100);
    opacityLabel_ = new QLabel("30%", this);
    opacityLabel_->setMinimumWidth(50);
    opacityLayout->addWidget(opacitySlider_);
    opacityLayout->addWidget(opacityLabel_);
    styleLayout->addRow("透明度:", opacityLayout);

    // 旋转角度
    QHBoxLayout* rotationLayout = new QHBoxLayout();
    rotationSlider_ = new QSlider(Qt::Horizontal, this);
    rotationSlider_->setRange(-90, 90);
    rotationLabel_ = new QLabel("45°", this);
    rotationLabel_->setMinimumWidth(50);
    rotationLayout->addWidget(rotationSlider_);
    rotationLayout->addWidget(rotationLabel_);
    styleLayout->addRow("旋转角度:", rotationLayout);

    // 密度
    densityComboBox_ = new QComboBox(this);
    densityComboBox_->addItem("稀疏", "sparse");
    densityComboBox_->addItem("中等", "medium");
    densityComboBox_->addItem("密集", "dense");
    styleLayout->addRow("密度:", densityComboBox_);

    mainLayout->addWidget(styleGroup);

    // === 对话框按钮 ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    detectLibreOfficeButton_ = new QPushButton("检测 LibreOffice", this);
    buttonLayout->addWidget(detectLibreOfficeButton_);
    buttonLayout->addStretch();
    saveButton_ = new QPushButton("保存", this);
    cancelButton_ = new QPushButton("取消", this);
    buttonLayout->addWidget(saveButton_);
    buttonLayout->addWidget(cancelButton_);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(addButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onAddKeywordClicked);
    connect(editButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onEditKeywordClicked);
    connect(deleteButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onDeleteKeywordClicked);
    connect(detectLibreOfficeButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onDetectLibreOfficeClicked);
    connect(saveButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onSaveClicked);
    connect(cancelButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onCancelClicked);
    connect(fontSizeSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged), this, &WatermarkSettingsDialog::onFontSizeChanged);
    connect(opacitySlider_, &QSlider::valueChanged, this, &WatermarkSettingsDialog::onOpacityChanged);
    connect(rotationSlider_, &QSlider::valueChanged, this, &WatermarkSettingsDialog::onRotationChanged);
    connect(colorButton_, &QPushButton::clicked, this, &WatermarkSettingsDialog::onColorButtonClicked);
    connect(enableWatermarkCheckBox_, &QCheckBox::toggled, this, &WatermarkSettingsDialog::onEnableWatermarkToggled);
}

void WatermarkSettingsDialog::loadSettings() {
    if (!watermarkService_) {
        return;
    }

    WatermarkService::WatermarkConfig config = watermarkService_->getConfig();

    // 加载全局启用状态
    enableWatermarkCheckBox_->setChecked(config.enabled);

    // 加载统一水印设置
    enableUnifiedCheckBox_->setChecked(config.enableUnified);
    unifiedTextEdit_->setText(config.unifiedText);

    // 加载水印样式
    fontSizeSpinBox_->setValue(config.fontSize);
    currentColor_ = config.color;
    updateColorButton();
    opacitySlider_->setValue(config.opacity);
    rotationSlider_->setValue(config.rotation);

    // 设置密度下拉框
    int densityIndex = densityComboBox_->findData(config.density);
    if (densityIndex >= 0) {
        densityComboBox_->setCurrentIndex(densityIndex);
    }

    // 加载关键词列表
    updateKeywordsTable();

    // 更新启用状态
    onEnableWatermarkToggled(config.enabled);
}

void WatermarkSettingsDialog::saveSettings() {
    if (!watermarkService_) {
        return;
    }

    WatermarkService::WatermarkConfig config;
    config.enabled = enableWatermarkCheckBox_->isChecked();
    config.fontSize = fontSizeSpinBox_->value();
    config.color = currentColor_;
    config.opacity = opacitySlider_->value();
    config.rotation = rotationSlider_->value();
    config.density = densityComboBox_->currentData().toString();
    config.enableUnified = enableUnifiedCheckBox_->isChecked();
    config.unifiedText = unifiedTextEdit_->text();

    watermarkService_->setConfig(config);

    // 保存关键词（从表格读取）
    QList<WatermarkService::KeywordRule> keywords;
    for (int row = 0; row < keywordsTable_->rowCount(); ++row) {
        QString detectText = keywordsTable_->item(row, 0)->text();
        QString watermarkText = keywordsTable_->item(row, 1)->text();
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(keywordsTable_->cellWidget(row, 2));
        bool enabled = checkBox ? checkBox->isChecked() : true;

        keywords.append(WatermarkService::KeywordRule(detectText, watermarkText, enabled));
    }
    watermarkService_->setKeywords(keywords);

    // 保存到配置文件
    watermarkService_->saveConfig();
}

void WatermarkSettingsDialog::updateKeywordsTable() {
    keywordsTable_->setRowCount(0);

    if (!watermarkService_) {
        return;
    }

    QList<WatermarkService::KeywordRule> keywords = watermarkService_->getKeywords();

    for (const WatermarkService::KeywordRule& rule : keywords) {
        int row = keywordsTable_->rowCount();
        keywordsTable_->insertRow(row);

        keywordsTable_->setItem(row, 0, new QTableWidgetItem(rule.detectText));
        keywordsTable_->setItem(row, 1, new QTableWidgetItem(rule.watermarkText));

        QCheckBox* checkBox = new QCheckBox(this);
        checkBox->setChecked(rule.enabled);
        keywordsTable_->setCellWidget(row, 2, checkBox);
    }
}

void WatermarkSettingsDialog::updateColorButton() {
    QString colorStyle = QString("background-color: %1; border: 1px solid #999;").arg(currentColor_.name());
    colorButton_->setStyleSheet(colorStyle);
    colorButton_->setText(currentColor_.name());
}

void WatermarkSettingsDialog::onAddKeywordClicked() {
    bool okDetect, okWatermark;
    QString detectText = QInputDialog::getText(this, "添加关键词", "检测文本:", QLineEdit::Normal, "", &okDetect);
    if (!okDetect || detectText.isEmpty()) {
        return;
    }

    QString watermarkText = QInputDialog::getText(this, "添加关键词", "水印文本:", QLineEdit::Normal, detectText, &okWatermark);
    if (!okWatermark || watermarkText.isEmpty()) {
        return;
    }

    int row = keywordsTable_->rowCount();
    keywordsTable_->insertRow(row);
    keywordsTable_->setItem(row, 0, new QTableWidgetItem(detectText));
    keywordsTable_->setItem(row, 1, new QTableWidgetItem(watermarkText));

    QCheckBox* checkBox = new QCheckBox(this);
    checkBox->setChecked(true);
    keywordsTable_->setCellWidget(row, 2, checkBox);
}

void WatermarkSettingsDialog::onEditKeywordClicked() {
    int currentRow = keywordsTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择要编辑的关键词");
        return;
    }

    QString oldDetectText = keywordsTable_->item(currentRow, 0)->text();
    QString oldWatermarkText = keywordsTable_->item(currentRow, 1)->text();

    bool okDetect, okWatermark;
    QString detectText = QInputDialog::getText(this, "编辑关键词", "检测文本:", QLineEdit::Normal, oldDetectText, &okDetect);
    if (!okDetect || detectText.isEmpty()) {
        return;
    }

    QString watermarkText = QInputDialog::getText(this, "编辑关键词", "水印文本:", QLineEdit::Normal, oldWatermarkText, &okWatermark);
    if (!okWatermark || watermarkText.isEmpty()) {
        return;
    }

    keywordsTable_->item(currentRow, 0)->setText(detectText);
    keywordsTable_->item(currentRow, 1)->setText(watermarkText);
}

void WatermarkSettingsDialog::onDeleteKeywordClicked() {
    int currentRow = keywordsTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择要删除的关键词");
        return;
    }

    QString detectText = keywordsTable_->item(currentRow, 0)->text();
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认删除",
        QString("确定要删除关键词 \"%1\" 吗？").arg(detectText),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        keywordsTable_->removeRow(currentRow);
    }
}

void WatermarkSettingsDialog::onSaveClicked() {
    saveSettings();
    QMessageBox::information(this, "保存成功", "水印设置已保存");
    accept();
}

void WatermarkSettingsDialog::onCancelClicked() {
    reject();
}

void WatermarkSettingsDialog::onFontSizeChanged(int value) {
    Q_UNUSED(value)
}

void WatermarkSettingsDialog::onOpacityChanged(int value) {
    opacityLabel_->setText(QString::number(value) + "%");
}

void WatermarkSettingsDialog::onRotationChanged(int value) {
    rotationLabel_->setText(QString::number(value) + "°");
}

void WatermarkSettingsDialog::onColorButtonClicked() {
    QColor color = QColorDialog::getColor(currentColor_, this, "选择水印颜色");
    if (color.isValid()) {
        currentColor_ = color;
        updateColorButton();
    }
}

void WatermarkSettingsDialog::onEnableWatermarkToggled(bool checked) {
    // 当全局禁用时，禁用其他设置
    keywordsTable_->setEnabled(checked);
    addButton_->setEnabled(checked);
    editButton_->setEnabled(checked);
    deleteButton_->setEnabled(checked);
    enableUnifiedCheckBox_->setEnabled(checked);
    unifiedTextEdit_->setEnabled(checked);
    fontSizeSpinBox_->setEnabled(checked);
    colorButton_->setEnabled(checked);
    opacitySlider_->setEnabled(checked);
    rotationSlider_->setEnabled(checked);
    densityComboBox_->setEnabled(checked);
}

void WatermarkSettingsDialog::onDetectLibreOfficeClicked() {
    QStringList possibleCommands = {"soffice", "soffice.exe", "libreoffice", "libreoffice.exe"};
    QString foundCommand;
    QString version;

    // 检测 LibreOffice
    for (const QString& cmd : possibleCommands) {
        QProcess testProcess;
        testProcess.start(cmd, QStringList() << "--version");
        if (testProcess.waitForStarted(2000)) {
            if (testProcess.waitForFinished(5000)) {
                foundCommand = cmd;
                version = QString::fromLocal8Bit(testProcess.readAllStandardOutput()).trimmed();
                break;
            }
        }
    }

    // 显示结果
    if (!foundCommand.isEmpty()) {
        QMessageBox::information(this, "LibreOffice 检测",
            QString("✅ 找到 LibreOffice！\n\n"
                    "命令: %1\n"
                    "版本: %2\n\n"
                    "水印功能可以正常使用。")
            .arg(foundCommand)
            .arg(version));
    } else {
        QMessageBox::warning(this, "LibreOffice 检测",
            QString("❌ 未找到 LibreOffice！\n\n"
                    "尝试的命令: %1\n\n"
                    "请按以下步骤操作：\n"
                    "1. 下载 LibreOffice: https://www.libreoffice.org/download/\n"
                    "2. 安装到系统（建议默认路径）\n"
                    "3. 将 LibreOffice 安装目录添加到系统 PATH 环境变量\n"
                    "   例如: C:\\Program Files\\LibreOffice\\program\n"
                    "4. 重启批办单查询系统服务器\n\n"
                    "没有 LibreOffice，水印功能将无法使用。")
            .arg(possibleCommands.join(", ")));
    }
}

}
