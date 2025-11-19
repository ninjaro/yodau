#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <memory>

#include "main_window.hpp"

#ifdef KC_KDE
#include <KAboutData>
#include <KLocalizedString>
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

#ifdef KC_KDE
    KLocalizedString::setApplicationDomain("yodau");

    KAboutData about_data(
        QStringLiteral("yodau"), QStringLiteral("yodau"), QStringLiteral("1.0"),
        QStringLiteral("YEAR OF THE DEPEND ADULT UNDERGARMENT"),
        KAboutLicense::MIT, QStringLiteral("(c) 2025, Yaroslav Riabtsev"),
        QString(), QStringLiteral("https://github.com/ninjaro/yodau"),
        QStringLiteral("yaroslav.riabtsev@rwth-aachen.de")
    );

    about_data.addAuthor(
        QStringLiteral("Yaroslav Riabtsev"), QStringLiteral("Original author"),
        QStringLiteral("yaroslav.riabtsev@rwth-aachen.de"),
        QStringLiteral("https://github.com/ninjaro"), QStringLiteral("ninjaro")
    );

    KAboutData::setApplicationData(about_data);

    QCommandLineParser parser;
    about_data.setupCommandLine(&parser);
    parser.process(app);
    about_data.processCommandLine(&parser);
#else
    QCoreApplication::setApplicationName(QStringLiteral("yodau"));
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A tool for card counting training.")
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
#endif

    auto window = std::make_unique<main_window>();
    window->show();

    int result = QApplication::exec();
    window.reset();
    return result;
}