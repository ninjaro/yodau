#ifndef YODAU_FRONTEND_MAIN_WINDOW_HPP
#define YODAU_FRONTEND_MAIN_WINDOW_HPP

#ifdef KC_KDE
#include <KXmlGuiWindow>
using BaseMainWindow = KXmlGuiWindow;
#else
#include <QMainWindow>
using BaseMainWindow = QMainWindow;
#endif

class board;
class settings_panel;
class QDockWidget;
class QStackedWidget;

class main_window final : public BaseMainWindow {
    Q_OBJECT
public:
    explicit main_window(QWidget* parent = nullptr);

private:
    board* main_zone;
    settings_panel* settings;

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QStackedWidget* zones_stack;
#else
    QDockWidget* settings_dock;
#endif
};

#endif // YODAU_FRONTEND_MAIN_WINDOW_HPP