#ifndef YODAU_APP_WIDGETS_LINE_WAVE_PREVIEW_HPP
#define YODAU_APP_WIDGETS_LINE_WAVE_PREVIEW_HPP

#include "shell/app_settings.hpp"

#include <QWidget>

/** Frozen synthetic hit using the live renderer, not a simulation or audio
 * scope. */
class line_wave_preview final : public QWidget {
    Q_OBJECT

public:
    explicit line_wave_preview(QWidget* parent = nullptr);
    void
    set_sample(const line_profile& profile, int age_ms, double display_gain);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    line_profile profile_;
    int age_ms_ { 200 };
    double display_gain_ { 1.0 };
};

#endif // YODAU_APP_WIDGETS_LINE_WAVE_PREVIEW_HPP
