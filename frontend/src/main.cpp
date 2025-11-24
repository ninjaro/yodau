#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <memory>

#include "helpers/str_label.hpp"
#include "main_window.hpp"

#ifdef KC_KDE
#include <KAboutData>
#include <KLocalizedString>
#endif

int main(int argc, char* argv[]) {
    const QApplication app(argc, argv);

#ifdef KC_KDE
    KLocalizedString::setApplicationDomain("yodau");

    KAboutData about_data(
        str_label("yodau"), str_label("yodau"), str_label("1.0"),
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

    QCommandLineParser parser;
    about_data.setupCommandLine(&parser);
    parser.process(app);
    about_data.processCommandLine(&parser);
#else
    QCoreApplication::setApplicationName(str_label("yodau"));
    QCoreApplication::setOrganizationName(str_label("yodau"));
    QCoreApplication::setApplicationVersion(str_label("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        str_label("YEAR OF THE DEPEND ADULT UNDERGARMENT")
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
#endif

    auto window = std::make_unique<main_window>();
    window->show();

    const int result = QApplication::exec();
    window.reset();
    return result;
}
