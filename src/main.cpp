#include "videoplayer.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[]) {



    QApplication a(argc, argv);
    VideoPlayer w;

    if (argc > 1) {//使用下面这个 可以读取中文和表情符号的文件
        QString filePath = QCoreApplication::arguments().at(1);
        w.openFile(filePath);
    }

    w.show();
    return a.exec();
}
