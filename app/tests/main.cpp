#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QtTest/QtTest>
#include <cstdio>

#include "ui/main_window_tests.hpp"

#ifdef KC_KDE
#include <KLocalizedString>
#endif

#if defined(Q_OS_LINUX) && !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
// Exercise the production entrypoint in a fresh subprocess, including its
// QApplication and unique_ptr cleanup. Only its symbol name changes; the
// close timer and diagnostic option belong to this runner.
#define main yodau_application_main_for_test
#include "../src/main.cpp"
#undef main

namespace {

bool application_close_probe = false;
bool probe_window_seen = false;
bool probe_checks_passed = false;
int probe_destructions = 0;
QPointer<main_window> probe_window;

void schedule_application_close_probe() {
    if (!application_close_probe) {
        return;
    }
    QTimer::singleShot(250, QCoreApplication::instance(), [] {
        for (auto* widget : QApplication::topLevelWidgets()) {
            auto* window = qobject_cast<main_window*>(widget);
            if (window == nullptr) {
                continue;
            }
            probe_window_seen = true;
            probe_window = window;
            QObject::connect(window, &QObject::destroyed, [] {
                ++probe_destructions;
            });
            probe_checks_passed
                = !window->testAttribute(Qt::WA_DeleteOnClose)
                && QApplication::quitOnLastWindowClosed();
            std::fprintf(
                stderr,
                "Application close probe: delete-on-close=%d "
                "last-window-quit=%d\n",
                window->testAttribute(Qt::WA_DeleteOnClose),
                QApplication::quitOnLastWindowClosed()
            );
            probe_checks_passed
                = window->close() && probe_checks_passed
                && !probe_window.isNull() && probe_destructions == 0;
            return;
        }
        // A missing window is a failed probe, rather than an indefinite wait.
        QCoreApplication::exit(1);
    });
}

Q_COREAPP_STARTUP_FUNCTION(schedule_application_close_probe)

} // namespace
#endif

int main(int argc, char** argv) {
#if defined(Q_OS_LINUX) && !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    if (argc == 2
        && qstrcmp(argv[1], "--probe-application-close") == 0) {
        application_close_probe = true;
        char* probe_arguments[] = { argv[0], nullptr };
        const int result = yodau_application_main_for_test(1, probe_arguments);
        std::fprintf(
            stderr,
            "Application cleanup probe: result=%d window-seen=%d "
            "pointer-cleared=%d destructions=%d\n",
            result, probe_window_seen, probe_window.isNull(), probe_destructions
        );
        return result == 0 && probe_window_seen && probe_checks_passed
                && probe_window.isNull() && probe_destructions == 1
            ? 0
            : 1;
    }
#endif

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
    return status;
}
