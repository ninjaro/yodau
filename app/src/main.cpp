#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <memory>

#include "shell/main_window.hpp"
#include "shell/str_label.hpp"

#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
#include "monitor/client.hpp"
#include "monitor/qt/gui_heartbeat.hpp"
#endif

#ifdef KC_KDE
#include <KAboutData>
#include <KLocalizedString>
#endif

#ifndef ECOSYSTEM_PROJECT_VERSION
#define ECOSYSTEM_PROJECT_VERSION "1.0.0"
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("yodau"));
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
    QCoreApplication::setApplicationVersion(
        QString::fromLatin1(ECOSYSTEM_PROJECT_VERSION)
    );
    QGuiApplication::setApplicationDisplayName(str_label("yodau"));
    app.setWindowIcon(
        QIcon::fromTheme(
            QStringLiteral("org.ninjaro.yodau"),
            QIcon(QStringLiteral(":/yodau/bug.svg"))
        )
    );
    QCommandLineParser parser;

#ifdef KC_KDE
    KLocalizedString::setApplicationDomain("yodau");
    QGuiApplication::setDesktopFileName(QStringLiteral("org.ninjaro.yodau"));

    KAboutData about_data(
        str_label("yodau"), str_label("yodau"),
        QString::fromLatin1(ECOSYSTEM_PROJECT_VERSION),
        str_label("YEAR OF THE DEPEND ADULT UNDERGARMENT"), KAboutLicense::MIT,
        str_label("(c) 2025, Yaroslav Riabtsev"), QString(),
        str_label("https://github.com/ninjaro/yodau"),
        str_label("yaroslav.riabtsev@rwth-aachen.de")
    );

    about_data.addAuthor(
        str_label("Yaroslav Riabtsev"), str_label("Original author"),
        str_label("yaroslav.riabtsev@rwth-aachen.de"),
        str_label("https://github.com/ninjaro"), str_label("ninjaro")
    );

    KAboutData::setApplicationData(about_data);

    about_data.setupCommandLine(&parser);
    parser.process(app);
    about_data.processCommandLine(&parser);
#else
    parser.setApplicationDescription(
        str_label("YEAR OF THE DEPEND ADULT UNDERGARMENT")
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
#endif

#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
    auto& watchdog = monitor::client::process();
    std::unique_ptr<monitor::qt::gui_heartbeat> gui_watchdog;
    if (watchdog.start("yodau")) {
        watchdog.breadcrumb(monitor::event::process_started);
        gui_watchdog
            = std::make_unique<monitor::qt::gui_heartbeat>(watchdog, &app);
    }
#endif

    auto window = std::make_unique<main_window>();
    window->show();

    const int result = QApplication::exec();
    window.reset();
#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
    gui_watchdog.reset();
    if (watchdog.available()) {
        watchdog.breadcrumb(monitor::event::process_stopping);
        watchdog.stop();
    }
#endif
    return result;
}
