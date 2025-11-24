#ifndef YODAU_FRONTEND_WIDGETS_BOARD_HPP
#define YODAU_FRONTEND_WIDGETS_BOARD_HPP

#include <QWidget>

class QString;
class QVBoxLayout;

class grid_view;
class stream_cell;

/**
 * @file board.hpp
 * @brief Declares the board widget that hosts grid and active stream views.
 *
 * The board is a top-level container combining:
 * - a grid of stream thumbnails (@ref grid_view),
 * - an optional active (focused) stream area containing a single
 *   @ref stream_cell.
 *
 * The active area is hidden when no stream is focused.
 */

/**
 * @brief Main stream layout widget: grid plus optional focused view.
 *
 * The board maintains two display modes simultaneously:
 * - **Grid mode**: all streams are shown in a @ref grid_view.
 * - **Active mode**: one stream is temporarily taken out of the grid
 *   and placed into an enlarged container at the top.
 *
 * Ownership model:
 * - The board owns the grid view and the active container.
 * - Stream cells are owned by the grid view most of the time.
 * - When a stream becomes active, its cell is *reparented* into the active
 *   container and later returned to the grid (no deletion involved).
 *
 * @note The class is final and not intended for subclassing.
 */
class board final : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the board widget.
     *
     * Creates:
     * - a @ref grid_view for thumbnail layout,
     * - an active container (initially hidden) that can host one active cell,
     * - an outer vertical layout stacking active container above the grid.
     *
     * @param parent Optional parent widget.
     */
    explicit board(QWidget* parent = nullptr);

    /**
     * @brief Access the grid view (thumbnail mode).
     *
     * @return Pointer to owned @ref grid_view.
     */
    grid_view* grid_mode() const;

    /**
     * @brief Get the currently active (focused) stream cell, if any.
     *
     * @return Active @ref stream_cell, or nullptr if none.
     */
    stream_cell* active_cell() const;

    /**
     * @brief Make a stream active by name.
     *
     * If a different stream is already active, it is returned to the grid
     * first. The requested cell is taken from the grid, reparented into the
     * active container, marked active, and enlarged.
     *
     * If @p name is empty or not found in grid, the call is ignored.
     *
     * @param name Name of the stream to focus.
     */
    void set_active_stream(const QString& name);

    /**
     * @brief Clear active mode and return the active cell to the grid.
     *
     * If no stream is active, this is a no-op.
     */
    void clear_active();

    /**
     * @brief Detach and return the active cell without putting it back to grid.
     *
     * The returned cell:
     * - is removed from the active layout,
     * - is marked inactive,
     * - remains parented to this board until the caller reparents it.
     *
     * After this call there is no active stream.
     *
     * @return The previously active cell, or nullptr if none.
     */
    stream_cell* take_active_cell();

private:
    /** @brief Grid view holding all non-active stream cells. */
    grid_view* grid;

    /** @brief Container widget for the active stream view. */
    QWidget* active_container;

    /** @brief Layout inside @ref active_container. */
    QVBoxLayout* active_layout;

    /** @brief Currently active stream cell (reparented into active container).
     */
    stream_cell* active_tile;
};

#endif // YODAU_FRONTEND_WIDGETS_BOARD_HPP