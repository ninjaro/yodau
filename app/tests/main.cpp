#include <QApplication>
#include <QtTest/QtTest>

#include "monitor/debug_probe_tests.hpp"
#include "ui/main_window_tests.hpp"

#ifdef KC_KDE
#include <KLocalizedString>
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("yodau-tests"));
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
#ifdef KC_KDE
    KLocalizedString::setApplicationDomain("yodau");
#endif

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
