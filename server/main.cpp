#include "ui/main_window.h"
#include "document_converter.h"
#include <QApplication>
#include <QMutex>
#include <QMetaObject>

// Global pointer to main window for message handler
static CrossNetShare::MainWindow* g_mainWindow = nullptr;
static QMutex g_logMutex;

// Custom message handler to redirect qDebug to UI
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QMutexLocker locker(&g_logMutex);

    QString formattedMessage;
    switch (type) {
    case QtDebugMsg:
        formattedMessage = msg;
        break;
    case QtInfoMsg:
        formattedMessage = "INFO: " + msg;
        break;
    case QtWarningMsg:
        formattedMessage = "WARNING: " + msg;
        break;
    case QtCriticalMsg:
        formattedMessage = "CRITICAL: " + msg;
        break;
    case QtFatalMsg:
        formattedMessage = "FATAL: " + msg;
        break;
    }

    // Send to main window log if available
    if (g_mainWindow) {
        QMetaObject::invokeMethod(g_mainWindow, [formattedMessage]() {
            if (g_mainWindow) {
                g_mainWindow->appendDebugLog(formattedMessage);
            }
        }, Qt::QueuedConnection);
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ApprovalSlipQueryServer");
    app.setOrganizationName("ApprovalSlipQuerySystem");

    // Install custom message handler
    qInstallMessageHandler(customMessageHandler);

    CrossNetShare::DocumentConverter::initialize();

    CrossNetShare::MainWindow window;
    g_mainWindow = &window;
    window.show();

    int result = app.exec();

    g_mainWindow = nullptr;
    CrossNetShare::DocumentConverter::cleanup();

    return result;
}
