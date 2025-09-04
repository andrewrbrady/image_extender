/**
 * @file main.cpp
 * @brief Main application entry point for the Extend Canvas UI
 */

#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("Extend Canvas");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Motive");

    // Create and show main window (start maximized to ensure ample width)
    MainWindow window;
    window.showMaximized();

    return app.exec();
}
