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

/**
 * @file main_window.hpp
 * @brief Declares the application's main top-level window.
 *
 * The main window hosts the primary UI:
 * - a central @ref board widget that shows streams,
 * - a @ref settings_panel that manages streams and lines.
 *
 * Platform-dependent layout:
 * - On desktop, settings are shown in a dock widget.
 * - On Android, board and settings are placed into a stacked view and can be
 *   swapped by the user via toolbar actions.
 */

/**
 * @brief Application main window with board and settings panel.
 *
 * The window is responsible for assembling the frontend object graph:
 * - creates backend @ref yodau::backend::stream_manager,
 * - creates UI widgets (@ref board and @ref settings_panel),
 * - creates a @ref controller that binds UI to backend,
 * - wires high-level signals from settings to controller slots.
 *
 * Lifetime rules:
 * - UI widgets are children of the window and are deleted with it.
 * - The backend stream manager is allocated on heap and explicitly deleted
 *   when the window is destroyed.
 * - The controller is a QObject child and follows Qt parent ownership.
 *
 * @note The class is final and not intended for subclassing.
 */
class main_window final : public BaseMainWindow {
    Q_OBJECT
public:
    /**
     * @brief Construct the main window and create all core UI components.
     *
     * Desktop:
     * - board is set as central widget;
     * - settings panel is placed into a right-side dock with a toolbar toggle.
     *
     * Android:
     * - board and settings are packed into a @ref QStackedWidget;
     * - the stack becomes the central widget.
     *
     * In all cases:
     * - a backend @ref yodau::backend::stream_manager is created;
     * - a @ref controller is constructed and connected;
     * - local sources detection is triggered once.
     *
     * @param parent Optional parent widget.
     */
    explicit main_window(QWidget* parent = nullptr);

private:
    /** @brief Central board widget showing streams (owned). */
    board* main_zone;

    /** @brief Settings side panel (owned). */
    settings_panel* settings;

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    /**
     * @brief Stacked container for mobile layouts.
     *
     * Holds the board and settings as separate pages.
     */
    QStackedWidget* zones_stack;
#else
    /**
     * @brief Dock widget hosting settings on desktop platforms.
     */
    QDockWidget* settings_dock;
#endif
};

#endif // YODAU_FRONTEND_MAIN_WINDOW_HPP