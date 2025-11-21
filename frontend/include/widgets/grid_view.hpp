#ifndef YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class stream_cell;

class grid_view final : public QWidget {
    Q_OBJECT

public:
    explicit grid_view(QWidget* parent = nullptr);

    void add_stream(const QString& name);
    void remove_stream(const QString& name);
    bool has_stream(const QString& name) const;

    QStringList stream_names() const;

signals:
    void stream_closed(const QString& name);
    void stream_enlarge(const QString& name);

private:
    void rebuild_layout();
    void close_requested(const QString& name);
    void enlarge_requested(const QString& name);

    QGridLayout* grid_layout;
    QMap<QString, stream_cell*> tiles;
};

#endif // YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
