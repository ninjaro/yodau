#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <memory>

#include "shell/main_window.hpp"
#include "shell/str_label.hpp"

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
    const QCommandLineOption monitor_option(
        QStringList() << QStringLiteral("m") << QStringLiteral("monitor"),
        QStringLiteral("Enable debug-only standalone monitor broadcasting.")
    );
    const QCommandLineOption monitor_endpoint_option(
        QStringList() << QStringLiteral("monitor-endpoint"),
        QStringLiteral(
            "Preferred local IPC endpoint name for debug monitor broadcasting."
        ),
        QStringLiteral("name")
    );

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
    parser.addOption(monitor_option);
    parser.addOption(monitor_endpoint_option);
    parser.process(app);
    about_data.processCommandLine(&parser);
#else
    parser.setApplicationDescription(
        str_label("YEAR OF THE DEPEND ADULT UNDERGARMENT")
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(monitor_option);
    parser.addOption(monitor_endpoint_option);
    parser.process(app);
#endif

    const QString env_monitor_endpoint
        = qEnvironmentVariable("YODAU_DEBUG_MONITOR_ENDPOINT");
    const QString monitor_endpoint
        = parser.isSet(QStringLiteral("monitor-endpoint"))
        ? parser.value(QStringLiteral("monitor-endpoint")).trimmed()
        : env_monitor_endpoint.trimmed();
    const bool enable_monitor = parser.isSet(QStringLiteral("monitor"))
        || !monitor_endpoint.isEmpty()
        || qEnvironmentVariableIntValue("YODAU_DEBUG_MONITOR") > 0;

    auto window
        = std::make_unique<main_window>(enable_monitor, monitor_endpoint);
    window->show();

    const int result = QApplication::exec();
    window.reset();
    return result;
}
