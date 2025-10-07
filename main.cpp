#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("Ball Game");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Game Developer");

    MainWindow window;
    window.show();

    return app.exec();
}
