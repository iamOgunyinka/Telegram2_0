#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Normaliz.lib")

#include "maindialog.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainDialog w;
    w.show();
    return a.exec();
}
