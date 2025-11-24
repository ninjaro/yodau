#ifndef YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QScrollArea;

class stream_cell;

/**
 * @file grid_view.hpp
 * @brief Declares a scrollable grid widget that hosts multiple stream cells.
 *
 * The grid_view arranges @ref stream_cell widgets in a responsive grid. It is
 * used as the "thumbnail mode" container by higher-level widgets (e.g. board).
 *
 * Stream cells are indexed by their logical names and can be added, removed,
 * temporarily taken out (for focusing), and put back.
 */

/**
 * @brief Scrollable grid of stream thumbnails.
 *
 * Responsibilities:
 * - Own and display a collection of @ref stream_cell widgets in a grid.
 * - Rebuild the layout when the set changes, choosing a roughly square grid.
 * - Forward user intents from individual cells via signals:
 *   - close requests -> @ref stream_closed
 *   - focus/enlarge requests -> @ref stream_enlarge
 *
 * Ownership model:
 * - grid_view owns all cells currently present in the grid.
 * - @ref take_stream_cell transfers a cell out of the grid without deleting it
 *   (caller is expected to reparent/place it elsewhere).
 * - @ref put_stream_cell returns a cell back into the grid.
 *
 * Threading:
 * - UI class; all methods are expected to be called from the Qt GUI thread.
 */
class grid_view final : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct an empty grid view.
     *
     * Creates an internal @ref QScrollArea with a container widget holding
     * a @ref QGridLayout. The scroll area is configured for horizontal
     * scrolling when needed and no vertical scrollbar (layout expands
     * horizontally).
     *
     * @param parent Optional parent widget.
     */
    explicit grid_view(QWidget* parent = nullptr);

    /**
     * @brief Check whether a stream with given name exists in the grid.
     *
     * @param name Stream name.
     * @return true if a cell with that name is present.
     */
    bool has_stream(const QString& name) const;

    /**
     * @brief Get names of all streams currently in the grid.
     *
     * @return List of stream names in insertion/order of the internal map.
     */
    QStringList stream_names() const;

    /**
     * @brief Add a new stream cell to the grid.
     *
     * If @p name is empty or already present, the call is ignored.
     * The created cell is connected to internal handlers and the layout
     * is rebuilt.
     *
     * @param name Logical stream name.
     */
    void add_stream(const QString& name);

    /**
     * @brief Remove a stream cell from the grid.
     *
     * If no such stream exists, this is a no-op. Otherwise the cell is removed
     * from the layout and scheduled for deletion with deleteLater().
     *
     * @param name Logical stream name.
     */
    void remove_stream(const QString& name);

    /**
     * @brief Detach a stream cell from the grid without deleting it.
     *
     * This is used when a stream becomes "active" elsewhere. The cell is:
     * - removed from internal map,
     * - removed from the layout,
     * - hidden,
     * - returned to the caller.
     *
     * Layout is rebuilt afterwards.
     *
     * @param name Stream name to take.
     * @return Pointer to the detached cell, or nullptr if not found.
     */
    stream_cell* take_stream_cell(const QString& name);

    /**
     * @brief Return a previously taken cell back into the grid.
     *
     * The cell is reparented to the grid container, inserted by its name,
     * shown, and the layout is rebuilt. If a cell with the same name already
     * exists, the call is ignored.
     *
     * @param cell Cell to put back.
     */
    void put_stream_cell(stream_cell* cell);

    /**
     * @brief Get a pointer to a cell without removing it.
     *
     * @param name Stream name.
     * @return Pointer to the cell, or nullptr if not found.
     */
    stream_cell* peek_stream_cell(const QString& name) const;

signals:
    /**
     * @brief Emitted after a stream cell was closed and removed.
     *
     * Triggered by the close button in a cell.
     *
     * @param name Name of the closed stream.
     */
    void stream_closed(const QString& name);

    /**
     * @brief Emitted when a stream cell requests focus/enlargement.
     *
     * Triggered by the focus/enlarge button in a cell.
     *
     * @param name Name of the stream to enlarge.
     */
    void stream_enlarge(const QString& name);

private:
    /**
     * @brief Recompute and rebuild the grid layout.
     *
     * Clears existing layout items, hides widgets during rebuild,
     * and places all current tiles into a grid with approximately
     * sqrt(n) columns, stretched equally.
     *
     * If there are no tiles, the widget hides itself.
     */
    void rebuild_layout();

    /**
     * @brief Internal handler for stream_cell::request_close.
     *
     * Removes the stream and emits @ref stream_closed.
     *
     * @param name Stream name.
     */
    void close_requested(const QString& name);

    /**
     * @brief Internal handler for stream_cell::request_focus.
     *
     * Emits @ref stream_enlarge.
     *
     * @param name Stream name.
     */
    void enlarge_requested(const QString& name);

    /** @brief Scroll area wrapping the grid container. */
    QScrollArea* scroll;

    /** @brief Widget that owns the grid layout and all tiles. */
    QWidget* grid_container;

    /** @brief Grid layout arranging tiles. */
    QGridLayout* grid_layout;

    /** @brief Map of stream name to corresponding tile widget. */
    QMap<QString, stream_cell*> tiles;
};

#endif // YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP