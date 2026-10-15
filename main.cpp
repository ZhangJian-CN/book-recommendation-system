// 程序启动入口。准备数据目录 -> 创建主窗口 -> 进入 Qt 事件循环。
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // QApplication 管理图形程序的事件分发；窗口显示后依靠事件循环持续响应用户操作。
    QApplication a(argc, argv);

    // --data-dir 指定可写数据副本；未指定时使用可执行文件旁的 data 目录。
    // 演示用独立目录可保留原始样例；重启传入同一目录，才能检查之前的保存结果。
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("CSV 数据目录"),
                      QStringLiteral("path")});
    parser.process(a);
    const QString dataPath = parser.isSet(QStringLiteral("data-dir"))
                                 ? parser.value(QStringLiteral("data-dir"))
                                 : a.applicationDirPath() + QStringLiteral("/data");
    QDir dataDir(dataPath);
    // 新目录或完全空目录首次启动时复制四份内置样例；已有数据目录继续使用原文件。
    if (!dataDir.exists()
        || dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        if (!QDir().mkpath(dataPath)) {
            QMessageBox::critical(nullptr, QStringLiteral("启动失败"),
                                  QStringLiteral("无法创建数据目录：%1").arg(dataPath));
            return 1;
        }
        for (const QString &name : {QStringLiteral("books.csv"),
                                    QStringLiteral("users.csv"),
                                    QStringLiteral("ratings.csv"),
                                    QStringLiteral("borrow_records.csv")}) {
            const QString target = dataDir.filePath(name);
            // :/seed/ 是编译进程序的资源；复制成磁盘文件后加写权限，供评分、修改和借还保存。
            if (!QFile::copy(QStringLiteral(":/seed/") + name, target)
                || !QFile::setPermissions(target, QFile::permissions(target)
                                                      | QFileDevice::WriteOwner
                                                      | QFileDevice::WriteUser)) {
                QMessageBox::critical(nullptr, QStringLiteral("启动失败"),
                                      QStringLiteral("无法初始化示例数据：%1").arg(name));
                return 1;
            }
        }
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "BookRecommendation_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    // MainWindow 构造函数加载 CSV 并展示数据；事件循环持续接收点击、输入和窗口事件。
    // w.show() 让窗口可见；exec() 等待事件直到退出程序，并不只执行一次按钮操作。
    MainWindow w(dataDir.absolutePath());
    w.show();
    return QApplication::exec();
}
