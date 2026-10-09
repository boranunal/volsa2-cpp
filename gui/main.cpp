/**
 * @file main.cpp
 * @brief GUI Application entry point for Volsa 2.
 * @details Initializes QApplication with dark palette styling, window titles,
 *          and instantiates the primary MainWindow.
 * @date 2026
 */

#include "mainwindow.hpp"
#include <QApplication>

/**
 * @brief Main routine for the Qt GUI executable.
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return Application exit code returned by exec().
 */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("VolSa2");
    app.setOrganizationName("Volsa2");
    app.setApplicationDisplayName("VolSa 2 - KORG Volca Sample 2 Manager");

    MainWindow window;
    window.show();

    return app.exec();
}
