#ifndef YODAU_APP_WIDGETS_FILE_TRANSPORT_HPP
#define YODAU_APP_WIDGETS_FILE_TRANSPORT_HPP

#include <QMetaObject>
#include <QPointer>
#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class stream_cell;

/** Active-file controls over the cell's existing player, not a processing
 * clock. */
class file_transport final : public QWidget {
    Q_OBJECT

public:
    explicit file_transport(QWidget* parent = nullptr);
    void set_stream(stream_cell* cell);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();
    void cancel_seek();
    [[nodiscard]] bool can_play() const;
    [[nodiscard]] qint64 seek_position(int step) const;
    void show_time(qint64 position);

    QPointer<stream_cell> cell_;
    std::vector<QMetaObject::Connection> connections_;
    QPushButton* play_ { nullptr };
    QPushButton* restart_ { nullptr };
    QComboBox* rate_ { nullptr };
    QSlider* seek_ { nullptr };
    QLabel* time_ { nullptr };
    QLabel* status_ { nullptr };
};

#endif // YODAU_APP_WIDGETS_FILE_TRANSPORT_HPP
