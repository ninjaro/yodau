#ifndef YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP

#include <QColor>
#include <QGroupBox>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * @file settings_panel.hpp
 * @brief Declares the settings side panel widget.
 *
 * The settings panel provides three main tabs:
 * - **Add stream**: UI for creating new streams from a file, local
 * camera/device, or a network URL.
 * - **Streams**: list of known streams with per-stream visibility toggles and
 *   a simple event log.
 * - **Active**: controls for selecting an active stream, choosing edit mode
 *   (draw new line vs. use template), editing draft line parameters, and adding
 *   templates/lines to the active stream. Includes an active log view.
 *
 * This widget emits high-level signals describing user intent. The controller
 * is expected to connect these signals to backend actions and to call the
 * public "setter" methods to keep UI state in sync with the backend.
 */

/**
 * @brief Side panel that exposes stream and line controls to the user.
 *
 * The panel itself is a pure UI component:
 * - It stores minimal UI state (existing names, active color selections, etc.).
 * - It does not access backend directly.
 * - It is driven by a controller that listens to signals and calls setters.
 *
 * Threading:
 * - This is a QWidget and must only be used from the Qt GUI thread.
 */
class settings_panel final : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the settings panel.
     *
     * Builds all UI tabs and initializes default mode to "file".
     *
     * @param parent Optional parent widget.
     */
    explicit settings_panel(QWidget* parent = nullptr);

    // existing names

    /**
     * @brief Replace the set of existing (reserved) names.
     *
     * Used to validate uniqueness of new stream/line/template names.
     *
     * @param names Set of names already used by backend/UI.
     */
    void set_existing_names(QSet<QString> names);

    /**
     * @brief Add one name to the existing-name set.
     *
     * @param name Name to reserve.
     */
    void add_existing_name(const QString& name);

    /**
     * @brief Remove one name from the existing-name set.
     *
     * @param name Name to unreserve.
     */
    void remove_existing_name(const QString& name);

    // streams tab

    /**
     * @brief Add a stream row to the streams list.
     *
     * If the stream already exists in the list, the call is ignored.
     *
     * @param name Stream name.
     * @param source Human-readable source description (type:path/url).
     * @param checked Initial "show in grid" state.
     */
    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;

    /**
     * @brief Set the "show" checkbox state for a stream entry.
     *
     * @param name Stream name.
     * @param checked New checked state.
     */
    void set_stream_checked(const QString& name, bool checked) const;

    /**
     * @brief Remove a stream entry from the streams list.
     *
     * @param name Stream name to remove.
     */
    void remove_stream_entry(const QString& name) const;

    /**
     * @brief Remove all stream entries and clear name reservations.
     */
    void clear_stream_entries();

    /**
     * @brief Append a line to the streams-tab event log.
     *
     * Prepends the current timestamp automatically.
     *
     * @param text Log line.
     */
    void append_event(const QString& text) const;

    // add tab

    /**
     * @brief Set detected local sources (e.g., /dev/video*).
     *
     * Updates the local-sources combo box.
     *
     * @param sources List of local source identifiers.
     */
    void set_local_sources(const QStringList& sources) const;

    /**
     * @brief Clear all add-tab input fields and reset validation.
     */
    void clear_add_inputs() const;

    /**
     * @brief Append a line to the add-tab log.
     *
     * @param text Log line.
     */
    void append_add_log(const QString& text) const;

    // active tab: streams

    /**
     * @brief Set the list of streams that can be selected as active.
     *
     * Ensures the current selection remains valid if possible,
     * otherwise falls back to "none".
     *
     * @param names Candidate active stream names.
     */
    void set_active_candidates(const QStringList& names) const;

    /**
     * @brief Programmatically select the active stream.
     *
     * If @p name is empty or not among candidates, selects "none".
     *
     * @param name Stream name to select as active, or empty to clear.
     */
    void set_active_current(const QString& name) const;

    // active tab: templates / lines

    /**
     * @brief Add one template name as a candidate in the templates combo.
     *
     * If already present, no change is made.
     *
     * @param name Template name to add.
     */
    void add_template_candidate(const QString& name) const;

    /**
     * @brief Replace the list of template candidates.
     *
     * Removes duplicates, inserts a "none" item, and selects it by default.
     *
     * @param names Template candidate names.
     */
    void set_template_candidates(const QStringList& names) const;

    /**
     * @brief Reset the "new line" form in the active tab.
     *
     * Clears name, sets closed=false, resets color to red, and emits
     * @ref active_line_params_changed to reflect the reset.
     */
    void reset_active_line_form();

    /**
     * @brief Reset the templates form to "none" selection.
     */
    void reset_active_template_form();

    /**
     * @brief Programmatically set active draft "closed" checkbox.
     *
     * This does not emit signals.
     *
     * @param closed New closed state.
     */
    void set_active_line_closed(bool closed) const;

    /**
     * @brief Get currently selected template name.
     *
     * @return Template name, trimmed. May be "none" or empty.
     */
    QString active_template_current() const;

    /**
     * @brief Get current preview color for templates.
     *
     * @return Preview color.
     */
    QColor active_template_preview_color() const;

    /**
     * @brief Append a line to the active-tab log.
     *
     * Prepends the current timestamp automatically.
     *
     * @param msg Log line.
     */
    void append_active_log(const QString& msg) const;

    /**
     * @brief Clear the active-tab log view.
     */
    void clear_active_log() const;

signals:
    // add tab

    /**
     * @brief User requested adding a file stream.
     *
     * @param path File path.
     * @param name Desired stream name (may be empty).
     * @param loop Whether to loop playback.
     */
    void add_file_stream(const QString& path, const QString& name, bool loop);

    /**
     * @brief User requested adding a local capture stream.
     *
     * @param source Local device identifier/path.
     * @param name Desired stream name (may be empty).
     */
    void add_local_stream(const QString& source, const QString& name);

    /**
     * @brief User requested adding a URL stream.
     *
     * @param url Network URL.
     * @param name Desired stream name (may be empty).
     */
    void add_url_stream(const QString& url, const QString& name);

    /**
     * @brief User requested re-detection of local sources.
     */
    void detect_local_sources_requested();

    // streams tab

    /**
     * @brief Emitted when a stream's "show in grid" state changes.
     *
     * @param name Stream name.
     * @param show New visibility state.
     */
    void show_stream_changed(const QString& name, bool show);

    // active tab

    /**
     * @brief Emitted when the active stream selection changes.
     *
     * Empty name indicates "none".
     *
     * @param name Active stream name or empty.
     */
    void active_stream_selected(const QString& name);

    /**
     * @brief Emitted when edit mode changes.
     *
     * @param drawing_new true for "draw new line", false for "use template".
     */
    void active_edit_mode_changed(bool drawing_new);

    /**
     * @brief Emitted when any draft-line parameter changes.
     *
     * @param name Draft/template name (may be empty).
     * @param color Draft color.
     * @param closed Whether the draft is closed.
     */
    void active_line_params_changed(
        const QString& name, const QColor& color, bool closed
    );

    /**
     * @brief Emitted when user clicks "add line" in active tab.
     *
     * @param name Desired line name (may be empty).
     * @param closed Whether the line is closed.
     */
    void active_line_save_requested(const QString& name, bool closed);

    /**
     * @brief Emitted when user requests undo of the last draft point.
     */
    void active_line_undo_requested();

    /**
     * @brief Emitted when label visibility toggle changes.
     *
     * @param on true to enable labels in active view.
     */
    void active_labels_enabled_changed(bool on);

    /**
     * @brief Emitted when template selection changes.
     *
     * Empty name indicates "none".
     *
     * @param template_name Selected template name or empty.
     */
    void active_template_selected(const QString& template_name);

    /**
     * @brief Emitted when template preview color changes.
     *
     * @param color New preview color.
     */
    void active_template_color_changed(const QColor& color);

    /**
     * @brief Emitted when user adds a template instance to active stream.
     *
     * @param template_name Template to add.
     * @param color Color to render that instance with.
     */
    void active_template_add_requested(
        const QString& template_name, const QColor& color
    );

private:
    /**
     * @brief Input mode for the add-tab.
     */
    enum class input_mode { file, local, url };

    // ui build
    /** @brief Build all tabs and root layout. */
    void build_ui();
    /** @brief Build the "add stream" tab. */
    QWidget* build_add_tab();
    /** @brief Build the "streams" tab. */
    QWidget* build_streams_tab();
    /** @brief Build the "active" tab. */
    QWidget* build_active_tab();

    /** @brief Build active-stream selection box. */
    QWidget* build_active_stream_box(QWidget* parent);
    /** @brief Build edit-mode radio group. */
    QWidget* build_edit_mode_box(QWidget* parent);
    /** @brief Build "new line" form box. */
    QWidget* build_new_line_box(QWidget* parent);
    /** @brief Build templates form box. */
    QWidget* build_templates_box(QWidget* parent);

    // add tab helpers
    /** @brief Switch add-tab mode and refresh enabled/visible controls. */
    void set_mode(input_mode mode);
    /** @brief Update visibility/enabled state of add-tab tool boxes. */
    void update_add_tools() const;
    /** @brief Update enabled state of the "add" button based on validation. */
    void update_add_enabled() const;

    /** @brief File chooser handler. */
    void on_choose_file();
    /** @brief "Add" button handler. */
    void on_add_clicked();
    /** @brief "Refresh local sources" handler. */
    void on_refresh_local();
    /** @brief Handler for name edit changes (validation). */
    void on_name_changed(QString text) const;

    /** @brief Resolve the name for the current input (trimmed). */
    QString resolved_name_for_current_input() const;
    /** @brief Check if @p name is not already reserved. */
    bool name_is_unique(const QString& name) const;
    /** @brief Validate that required input fields for current mode are filled.
     */
    bool current_input_valid() const;

    /** @brief Apply/remove UI error styling for name edit. */
    void set_name_error(bool error) const;

    // active tab helpers
    /** @brief Show/hide active-tab tool boxes based on selection/mode. */
    void update_active_tools() const;
    /** @brief Paint a QPushButton background to match a chosen color. */
    void set_btn_color(QPushButton* btn, const QColor& c) const;

private slots:
    /** @brief Active-stream combo change handler. */
    void on_active_combo_changed(const QString& text);
    /** @brief Edit-mode radio click handler. */
    void on_active_mode_clicked(int id);

    /** @brief "Choose line color" click handler. */
    void on_active_line_color_clicked();
    /** @brief "Undo point" click handler. */
    void on_active_line_undo_clicked();
    /** @brief "Add line" click handler. */
    void on_active_line_save_clicked();
    /** @brief Line name edit finished handler. */
    void on_active_line_name_finished();
    /** @brief Closed-checkbox toggle handler. */
    void on_active_line_closed_toggled(bool checked);

    /** @brief Template combo change handler. */
    void on_active_template_combo_changed(const QString& text);
    /** @brief "Choose template color" click handler. */
    void on_active_template_color_clicked();
    /** @brief "Add template" click handler. */
    void on_active_template_add_clicked();

    /** @brief Streams list item change handler (checkbox column). */
    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    // common
    /** @brief Tab widget hosting add/streams/active tabs. */
    QTabWidget* tabs;
    /** @brief Reserved names used for uniqueness checks. */
    QSet<QString> existing_names;

    // add tab
    QWidget* add_tab;
    QLineEdit* name_edit;

    QButtonGroup* mode_group;
    QRadioButton* file_radio;
    QRadioButton* local_radio;
    QRadioButton* url_radio;
    input_mode current_mode;

    QGroupBox* add_file_box = nullptr;
    QLineEdit* file_path_edit;
    QPushButton* choose_file_btn;
    QCheckBox* loop_checkbox;

    QGroupBox* add_local_box = nullptr;
    QComboBox* local_sources_combo;
    QPushButton* refresh_local_btn;

    QGroupBox* add_url_box = nullptr;
    QLineEdit* url_edit;

    QPushButton* add_btn;
    QPlainTextEdit* add_log_view;

    // streams tab
    QWidget* streams_tab;
    QTreeWidget* streams_list;
    QPlainTextEdit* event_log_view;

    // active tab
    QWidget* active_tab { nullptr };
    QComboBox* active_combo { nullptr };
    QCheckBox* active_labels_cb = nullptr;

    QGroupBox* active_mode_box = nullptr;
    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };

    QGroupBox* active_line_box { nullptr };
    QLineEdit* active_line_name_edit { nullptr };
    QCheckBox* active_line_closed_cb { nullptr };
    QPushButton* active_line_color_btn { nullptr };
    QColor active_line_color { Qt::red };
    QPushButton* active_line_undo_btn { nullptr };
    QPushButton* active_line_save_btn { nullptr };

    QGroupBox* active_templates_box { nullptr };
    QComboBox* active_template_combo { nullptr };
    QPushButton* active_template_color_btn { nullptr };
    QColor active_template_color { Qt::red };
    QPushButton* active_template_add_btn { nullptr };

    QPlainTextEdit* active_log_view = nullptr;
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
