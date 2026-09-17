#include "db_manager.h"
#include "document_converter.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

namespace CrossNetShare {

static DbManager* g_instance = nullptr;

DbManager* DbManager::instance() {
    if (!g_instance) {
        g_instance = new DbManager();
    }
    return g_instance;
}

DbManager::DbManager(QObject* parent)
    : QObject(parent)
{
}

DbManager::~DbManager() {
}

QString DbManager::findPython() const {
    return DocumentConverter::findPython();
}

QString DbManager::findAppDir() const {
    return QCoreApplication::applicationDirPath();
}

QMap<QString, QString> DbManager::runPythonJson(const QStringList& args, QString& error) const {
    QMap<QString, QString> result;
    QString python = findPython();
    if (python.isEmpty()) {
        error = "未找到 Python 解释器";
        return result;
    }

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("JAVA_TOOL_OPTIONS", "-Dfile.encoding=UTF-8");
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);

    QStringList allArgs = args;
    process.start(python, allArgs);

    if (!process.waitForStarted(15000)) {
        error = "启动 Python 失败: " + process.errorString();
        return result;
    }

    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished(5000);
        error = "Python 执行超时";
        return result;
    }

    QString stdout_str = QString::fromUtf8(process.readAllStandardOutput());
    QString stderr_str = QString::fromUtf8(process.readAllStandardError());

    if (process.exitCode() != 0) {
        error = QString("Python 返回错误 %1: %2").arg(process.exitCode()).arg(stderr_str);
        return result;
    }

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(stdout_str.toUtf8(), &perr);
    if (doc.isNull()) {
        error = "JSON 解析失败: " + perr.errorString() + "\n输出: " + stdout_str;
        return result;
    }

    result["json"] = stdout_str;
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        result["success"] = obj.value("success").toBool() ? "true" : "false";
    }
    return result;
}

static ApprovalRecord jsonObjToRecord(const QJsonObject& obj) {
    ApprovalRecord r;
    auto s = [&](const char* k) { return obj.value(k).toVariant().toString(); };
    r.id = s("ID");
    r.receiveNumber = s("RECEIVE_NUMBER");
    r.department = s("DEPARTMENT");
    r.wordCode = s("WORD_CODE");
    r.fileReceiveDate = s("FILE_RECEIVE_DATE");
    r.fileCategory = s("FILE_CATEGORY");
    r.receiveChannel = s("RECEIVE_CHANNEL");
    r.emergencyLevel = s("EMERGENCY_LEVEL");
    r.secretLevel = s("SECRET_LEVEL");
    r.summary = s("SUMMARY");
    r.leaderInstruction = s("LEADER_INSTRUCTION");
    r.suggestion = s("SUGGESTION");
    r.processResult = s("PROCESS_RESULT");
    r.organizer = s("ORGANIZER");
    r.notes = s("NOTES");
    r.createTime = s("CREATE_TIME");
    r.modifyTime = s("MODIFY_TIME");
    r.rawJson = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return r;
}

bool DbManager::parseRecordsJson(const QString& jsonText, QList<ApprovalRecord>& outRecords) const {
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &perr);
    if (doc.isNull()) {
        qWarning() << "[DbManager] parseRecordsJson error:" << perr.errorString();
        return false;
    }
    QJsonObject root = doc.object();
    if (!root.value("success").toBool()) {
        return false;
    }
    QJsonArray arr = root.value("records").toArray();
    for (const QJsonValue& v : arr) {
        if (v.isObject()) {
            outRecords.append(jsonObjToRecord(v.toObject()));
        }
    }
    return true;
}

bool DbManager::setDatabasePath(const QString& dbBasePath) {
    QMutexLocker lock(&mutex_);
    bool isRemote = dbBasePath.startsWith("tcp://");
    if (!isRemote) {
        QString mvDb = dbBasePath + ".mv.db";
        QString sqliteDb = dbBasePath + ".db";
        if (!QFileInfo::exists(mvDb) && !QFileInfo::exists(sqliteDb)) {
            emit error("找不到数据库文件: " + mvDb + " 或 " + sqliteDb);
            return false;
        }
    }
    activeDbPath_ = dbBasePath;
    if (!databasePaths_.contains(dbBasePath)) {
        databasePaths_.append(dbBasePath);
        emit databaseListChanged();
    }
    lock.unlock();
    bool ok = refresh();
    if (ok) {
        emit databaseChanged();
    }
    return ok;
}

bool DbManager::addDatabase(const QString& dbBasePath) {
    bool isRemote = dbBasePath.startsWith("tcp://");
    if (!isRemote) {
        QString mvDb = dbBasePath + ".mv.db";
        QString sqliteDb = dbBasePath + ".db";
        if (!QFileInfo::exists(mvDb) && !QFileInfo::exists(sqliteDb)) {
            emit error("找不到数据库文件: " + mvDb + " 或 " + sqliteDb);
            return false;
        }
    }
    QMutexLocker lock(&mutex_);
    if (!databasePaths_.contains(dbBasePath)) {
        databasePaths_.append(dbBasePath);
        emit databaseListChanged();
    }
    if (activeDbPath_.isEmpty()) {
        activeDbPath_ = dbBasePath;
        lock.unlock();
        if (refresh()) emit databaseChanged();
        return true;
    }
    return true;
}

bool DbManager::removeDatabase(const QString& dbBasePath) {
    QMutexLocker lock(&mutex_);
    databasePaths_.removeAll(dbBasePath);
    if (activeDbPath_ == dbBasePath) {
        activeDbPath_.clear();
        records_.clear();
        if (!databasePaths_.isEmpty()) {
            activeDbPath_ = databasePaths_.first();
            lock.unlock();
            if (refresh()) emit databaseChanged();
        } else {
            emit databaseChanged();
        }
    }
    emit databaseListChanged();
    return true;
}

QStringList DbManager::getDatabaseList() const {
    QMutexLocker lock(&mutex_);
    return databasePaths_;
}

bool DbManager::setActiveDatabase(const QString& dbBasePath) {
    QMutexLocker lock(&mutex_);
    if (!databasePaths_.contains(dbBasePath)) {
        return false;
    }
    activeDbPath_ = dbBasePath;
    lock.unlock();
    bool ok = refresh();
    if (ok) emit databaseChanged();
    return ok;
}

QString DbManager::getActiveDatabase() const {
    QMutexLocker lock(&mutex_);
    return activeDbPath_;
}

bool DbManager::refresh() {
    QMutexLocker lock(&mutex_);
    if (activeDbPath_.isEmpty()) {
        return false;
    }
    QString appDir = findAppDir();
    QString script = appDir + "/db_query.py";
    if (!QFileInfo::exists(script)) {
        script = QCoreApplication::applicationDirPath() + "/../server/db_query.py";
        script = QDir::cleanPath(script);
    }
    if (!QFileInfo::exists(script)) {
        qWarning() << "[DbManager] 找不到 db_query.py 脚本, 尝试使用源码目录";
        script = QDir::cleanPath(qApp->applicationDirPath() + "/../../server/db_query.py");
    }
    QString errStr;
    QStringList args;
    args << script << activeDbPath_ << "list";
    auto result = runPythonJson(args, errStr);
    if (result.isEmpty()) {
        qWarning() << "[DbManager] refresh failed:" << errStr;
        emit error("读取数据库失败: " + errStr);
        return false;
    }
    QList<ApprovalRecord> recs;
    if (!parseRecordsJson(result["json"], recs)) {
        emit error("解析数据库记录失败");
        return false;
    }
    records_ = recs;
    qDebug() << "[DbManager] 载入" << recs.size() << "条记录";
    return true;
}

QList<ApprovalRecord> DbManager::getAllRecords() const {
    QMutexLocker lock(&mutex_);
    return records_;
}

QList<ApprovalRecord> DbManager::searchRecords(const QString& keyword) const {
    QMutexLocker lock(&mutex_);
    if (activeDbPath_.isEmpty()) {
        return {};
    }
    // 使用脚本搜索
    QString appDir = findAppDir();
    QString script = appDir + "/db_query.py";
    if (!QFileInfo::exists(script)) {
        script = QDir::cleanPath(qApp->applicationDirPath() + "/../../server/db_query.py");
    }
    QString errStr;
    lock.unlock();
    QStringList args;
    args << script << activeDbPath_ << "search" << keyword;
    auto result = runPythonJson(args, errStr);
    if (result.isEmpty()) {
        return {};
    }
    QList<ApprovalRecord> recs;
    parseRecordsJson(result["json"], recs);
    return recs;
}

bool DbManager::getRecordById(const QString& id, ApprovalRecord& outRecord) const {
    QMutexLocker lock(&mutex_);
    for (const auto& r : records_) {
        if (r.id == id) {
            outRecord = r;
            return true;
        }
    }
    // 找不到时再通过脚本查询
    if (activeDbPath_.isEmpty()) return false;
    QString appDir = findAppDir();
    QString script = appDir + "/db_query.py";
    if (!QFileInfo::exists(script)) {
        script = QDir::cleanPath(qApp->applicationDirPath() + "/../../server/db_query.py");
    }
    QString errStr;
    lock.unlock();
    QStringList args;
    args << script << activeDbPath_ << "get" << id;
    auto result = runPythonJson(args, errStr);
    if (result.isEmpty()) return false;
    QJsonParseError perr;
    auto doc = QJsonDocument::fromJson(result["json"].toUtf8(), &perr);
    if (doc.isNull()) return false;
    QJsonObject obj = doc.object();
    if (!obj.value("success").toBool()) return false;
    QJsonObject recObj = obj.value("record").toObject();
    outRecord = jsonObjToRecord(recObj);
    return true;
}

QString DbManager::generateDocument(const ApprovalRecord& record,
                                     const QString& templatePath,
                                     QString& outputDocxPath,
                                     QString& outputPdfPath,
                                     QString& errorMessage) {
    QString python = findPython();
    if (python.isEmpty()) {
        errorMessage = "未找到 Python 解释器";
        return "";
    }
    QString appDir = findAppDir();
    QString script = appDir + "/fill_template_v2.py";
    if (!QFileInfo::exists(script)) {
        script = QDir::cleanPath(qApp->applicationDirPath() + "/../../server/fill_template_v2.py");
    }
    if (!QFileInfo::exists(script)) {
        errorMessage = "找不到 fill_template_v2.py 脚本: " + script;
        return "";
    }
    QString realTemplate = templatePath;
    if (!QFileInfo::exists(realTemplate)) {
        realTemplate = appDir + "/模板.docx";
    }
    if (!QFileInfo::exists(realTemplate)) {
        realTemplate = QDir::cleanPath(qApp->applicationDirPath() + "/../../模板.docx");
    }
    if (!QFileInfo::exists(realTemplate)) {
        errorMessage = "找不到 Word 模板文件: " + realTemplate;
        return "";
    }

    // 创建临时目录
    QTemporaryDir tempDir(QDir::temp().filePath("cns_fill_XXXXXX"));
    if (!tempDir.isValid()) {
        errorMessage = "无法创建临时目录";
        return "";
    }
    QString recordJsonPath = tempDir.path() + "/record.json";
    QFile rf(recordJsonPath);
    if (!rf.open(QIODevice::WriteOnly)) {
        errorMessage = "无法写入 record.json";
        return "";
    }
    rf.write(record.rawJson.toUtf8());
    rf.close();

    QString outputDir = tempDir.path();

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("JAVA_TOOL_OPTIONS", "-Dfile.encoding=UTF-8");
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);

    QStringList args;
    args << script << realTemplate << recordJsonPath << outputDir << appDir;
    process.start(python, args);

    if (!process.waitForStarted(15000)) {
        errorMessage = "启动填充脚本失败: " + process.errorString();
        return "";
    }
    if (!process.waitForFinished(180000)) {
        process.kill();
        process.waitForFinished(5000);
        errorMessage = "填充模板超时";
        return "";
    }
    QString stdout_str = QString::fromUtf8(process.readAllStandardOutput());
    QString stderr_str = QString::fromUtf8(process.readAllStandardError());
    if (process.exitCode() != 0) {
        errorMessage = QString("填充失败 exit=%1 stderr=%2").arg(process.exitCode()).arg(stderr_str);
        return "";
    }
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(stdout_str.toUtf8(), &perr);
    if (doc.isNull()) {
        errorMessage = "结果JSON解析失败: " + perr.errorString() + "\n" + stdout_str;
        return "";
    }
    QJsonObject obj = doc.object();
    if (!obj.value("success").toBool()) {
        errorMessage = "脚本返回失败";
        return "";
    }
    QString pdfPath = obj.value("pdf_path").toString();
    QString docxPath = obj.value("docx_path").toString();

    // v2不生成PDF，只返回DOCX
    if (docxPath.isEmpty()) {
        errorMessage = "脚本未返回docx_path";
        return "";
    }

    // 将文件复制到持久临时目录（否则 QTemporaryDir 析构会删）
    static int s_counter = 0;
    QString persistDir = QDir::temp().filePath(QString("cns_docs_%1_%2").arg(QCoreApplication::applicationPid()).arg(++s_counter));
    QDir().mkpath(persistDir);
    QString filename = obj.value("filename").toString();
    QString finalDocx = persistDir + "/" + filename + ".docx";
    QString finalPdf = persistDir + "/" + filename + ".pdf";
    if (QFile::exists(finalDocx)) QFile::remove(finalDocx);
    if (QFile::exists(finalPdf)) QFile::remove(finalPdf);
    if (!QFile::copy(docxPath, finalDocx)) {
        // fallback: 直接使用临时路径（用户关闭前仍存在）
        finalDocx = docxPath;
    }
    // PDF由v2版本不生成，留空（水印流程只需要DOCX）
    if (!pdfPath.isEmpty() && QFile::exists(pdfPath)) {
        if (!QFile::copy(pdfPath, finalPdf)) {
            finalPdf = pdfPath;
        }
    } else {
        finalPdf = "";  // v2不生成PDF
    }
    outputDocxPath = finalDocx;
    outputPdfPath = finalPdf;
    return finalDocx;  // 返回DOCX路径（水印流程使用）
}

}
