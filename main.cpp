
#include "mainwindow.h"

#include <QApplication>

extern "C"
{
#include <libavformat/avformat.h>
#include "libavdevice/avdevice.h"
}


extern int test_push();   // 声明刚才那个测试函数

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();

    // 直接运行推流测试，阻塞直到程序被 Ctrl+C 结束
    //int ret = test_push();
    //return ret;

}
