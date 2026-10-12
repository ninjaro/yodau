#ifndef YODAU_APP_WIDGETS_STREAM_BOARD_HPP
#define YODAU_APP_WIDGETS_STREAM_BOARD_HPP

#include <QWidget>

class QString;
class QBoxLayout;
class QVBoxLayout;

class grid_view;
class stream_cell;
class file_transport;

enum class pool_side { bottom, right, left };

class stream_board final : public QWidget {
    Q_OBJECT
public:
    explicit stream_board(QWidget* parent = nullptr);

    [[nodiscard]] grid_view* grid_mode() const;
    [[nodiscard]] stream_cell* active_cell() const;
    [[nodiscard]] pool_side current_pool_side() const;

    /// Recompose existing views without changing stream or grid preferences.
    void set_pool_side(pool_side side);
    void add_canvas_tools(QWidget* tools);
    void set_active_stream(const QString& name);
    void clear_active();
    stream_cell* take_active_cell();
    void set_application_active(bool active);

protected:
    void changeEvent(QEvent* event) override;

private:
    void update_pool_layout();
    grid_view* grid;

    QWidget* active_container;
    QVBoxLayout* active_layout;
    stream_cell* active_tile;
    file_transport* file_controls_ { nullptr };
    QBoxLayout* board_layout;
    pool_side pool_side_ { pool_side::bottom };
    bool application_active_ { true };
};

#endif // YODAU_APP_WIDGETS_STREAM_BOARD_HPP
