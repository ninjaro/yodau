#include <QApplication>
#include <QtTest/QtTest>

#include "include/debug_probe_tests.hpp"
#include "include/main_window_tests.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    int status = 0;

    {
        main_window_tests t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        debug_probe_tests t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}
