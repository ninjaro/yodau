#ifndef YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QMainWindow;
class QDockWidget;
class QScrollArea;
class QHBoxLayout;
class QVBoxLayout;
class stream_cell;

class carousel_view final : public QWidget {
    Q_OBJECT

public:
    explicit carousel_view(QWidget* parent = nullptr);

    void add_stream(const QString& name);
    void remove_stream(const QString& name);
    bool has_stream(const QString& name) const;

    QStringList stream_names() const;

    [[nodiscard]] QString active_stream() const;
    void set_active_stream(const QString& name);

signals:
    void stream_closed(const QString& name);
    void stream_enlarge(const QString& name);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild_strip();
    void close_requested(const QString& name);
    void focus_requested(const QString& name);
    void adjust_dock_height();

    QMainWindow* root;
    QWidget* active_container;
    QVBoxLayout* active_layout;

    QDockWidget* dock;
    QScrollArea* scroll;
    QWidget* strip_container;
    QHBoxLayout* strip_layout;

    QMap<QString, stream_cell*> tiles;

    QString active_name;
    stream_cell* active_tile { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP
