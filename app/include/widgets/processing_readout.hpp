#ifndef YODAU_APP_WIDGETS_PROCESSING_READOUT_HPP
#define YODAU_APP_WIDGETS_PROCESSING_READOUT_HPP

#include "shell/processing_sample.hpp"

#include <QWidget>
#include <functional>

class QHideEvent;
class QLabel;
class QShowEvent;
class QTimer;

class processing_readout final : public QWidget {
    Q_OBJECT
public:
    explicit processing_readout(QWidget* parent = nullptr);
    /// Refresh on showing/rebinding and once per second while visible; no
    /// history.
    void set_reader(std::function<processing_sample()> reader);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void refresh();
    std::function<processing_sample()> reader_;
    QTimer* timer_;
    QLabel* target_;
    QLabel* source_;
    QLabel* input_;
    QLabel* policy_;
    QLabel* algorithm_;
    QLabel* result_;
    QLabel* summary_;
};

#endif
