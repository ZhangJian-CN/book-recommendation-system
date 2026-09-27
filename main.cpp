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
    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("CSV 数据目录"),
                      QStringLiteral("path")});
    parser.process(a);
    const QString dataPath = parser.isSet(QStringLiteral("data-dir"))
                                 ? parser.value(QStringLiteral("data-dir"))
                                 : a.applicationDirPath() + QStringLiteral("/data");
    QDir dataDir(dataPath);
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
            if (!QFile::copy(QStringLiteral(":/seed/") + name, dataDir.filePath(name))) {
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
    MainWindow w(dataDir.absolutePath());
    w.show();
    return QApplication::exec();
}
