#ifndef YODAU_APP_WIDGETS_PROCESSING_READOUT_HPP
#define YODAU_APP_WIDGETS_PROCESSING_READOUT_HPP

#include "shell/processing_sample.hpp"

#include <shared_ui/qt/material.hpp>
#include <shared_ui/qt/readout_group.hpp>

#include <QList>
#include <QWidget>
#include <functional>

class QHideEvent;
class QShowEvent;
class QTimer;

/// The three inspector sections of one sample, in display order: active
/// stream, GUI observations, effective policy. Pure projection; it decides
/// what is unavailable, stale or retained, never how it is drawn.
[[nodiscard]] QList<QList<shared_ui::qt::readout_item>>
processing_readout_items(const processing_sample& sample);

class processing_readout final : public QWidget {
    Q_OBJECT
public:
    explicit processing_readout(QWidget* parent = nullptr);
    /// Refresh on showing/rebinding and once per second while visible; no
    /// history.
    void set_reader(std::function<processing_sample()> reader);
    /// The window style the sections are drawn in; presentation only.
    void set_presentation(
        shared_ui::qt::material value, shared_ui::qt::platform_profile profile
    );

    [[nodiscard]] QList<shared_ui::qt::readout_group*> sections() const {
        return sections_;
    }

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void refresh();
    std::function<processing_sample()> reader_;
    QTimer* timer_;
    QList<shared_ui::qt::readout_group*> sections_;
};

#endif
