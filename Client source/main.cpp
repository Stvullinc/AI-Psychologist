#include "mainwindow.h"
#include <QApplication>

// основная функция
int main(int argc, char *argv[]) {
    QApplication a(argc, argv); // инициализация объекта приложения
    MainWindow w; // инициализация окна диалога
    w.show(); // вывод окна на экран
    return a.exec(); // запуск приложения
}
