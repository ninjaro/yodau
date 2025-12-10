#ifndef YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QScrollArea;

class stream_cell;

class grid_view final : public QWidget {
    Q_OBJECT
public:
    explicit grid_view(QWidget* parent = nullptr);

    bool has_stream(const QString& name) const;
    QStringList stream_names() const;

    void add_stream(const QString& name);
    void remove_stream(const QString& name);

    stream_cell* take_stream_cell(const QString& name);
    void put_stream_cell(stream_cell* cell);
    stream_cell* peek_stream_cell(const QString& name) const;

signals:
    void stream_closed(const QString& name);
    void stream_enlarge(const QString& name);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild_layout();
    void close_requested(const QString& name);
    void enlarge_requested(const QString& name);

    QScrollArea* scroll;
    QWidget* grid_container;
    QGridLayout* grid_layout;
    QMap<QString, stream_cell*> tiles;
};

#endif // YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
