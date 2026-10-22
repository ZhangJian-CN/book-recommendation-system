// 【曹振江重点：程序启动与集成入口】
// 顺序：创建 QApplication -> 解析数据目录 -> 必要时复制样例 -> 创建窗口 -> 进入事件循环。
// main 只决定使用哪份数据，具体字段校验与业务行为交给窗口内的 LibraryStore。
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
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
    // --data-dir 后面的目录决定本次读写位置；重启验证必须继续使用同一个目录。
    parser.addHelpOption();
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("CSV 数据目录"),
                      QStringLiteral("path")});
    parser.process(a);
    const QString dataPath = parser.isSet(QStringLiteral("data-dir"))
                                 ? parser.value(QStringLiteral("data-dir"))
                                 : a.applicationDirPath() + QStringLiteral("/data");
    QDir dataDir(dataPath);
    // 初始化临界区与 LibraryStore 使用同一目录锁；释放后由窗口接管长时占用。
    QString initializationError;
    if (!LibraryStore::initializeDataDirectory(dataPath, &initializationError)) {
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), initializationError);
        return 1;
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) { // 按系统语言顺序寻找可用翻译，找到后安装并停止遍历。
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
