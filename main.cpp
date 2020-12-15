#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Normaliz.lib")

#include "maindialog.hpp"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  QTranslator translator{}, qt_translator{};

  translator.load( QString( "sdi_" ) + QLocale::system().name() );
  qt_translator.load( QString( "qt_") + QLocale::system().name() );

  a.installTranslator( &translator );
  a.installTranslator( &qt_translator );

  MainDialog w{};
  w.show();
  return a.exec();
}
