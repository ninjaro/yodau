#include "widgets/line_profile_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/line_wave_preview.hpp"
#include "widgets/line_wave_renderer.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <iterator>

namespace {

struct wave_preset {
    const char* width;
    const char* span;
    const char* response;
};

constexpr wave_preset presets[] {
    { "thin", "short", "dry" },
    { "medium", "medium", "balanced" },
    { "string_heavy", "long", "resonant" },
};

} // namespace

void line_profile_panel::build_wave_controls(QVBoxLayout* layout) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    Q_UNUSED(layout);
#else
    wave_checkbox_ = new QCheckBox(str_label("String / wave preview"), this);
    wave_checkbox_->setObjectName(QStringLiteral("line_wave_toggle"));
    wave_checkbox_->setAccessibleName(str_label("Show string wave preview"));
    layout->addWidget(wave_checkbox_);

    wave_controls_ = new QWidget(this);
    wave_controls_->setObjectName(QStringLiteral("line_wave_controls"));
    const auto controls = new QVBoxLayout(wave_controls_);
    controls->setContentsMargins(0, 0, 0, 0);

    const auto hint = new QLabel(
        str_label(
            "Synthetic hit on a sample path, not live geometry or audio. "
            "Presets change the existing width / span / damping fields. "
            "Automatic colours use a representative colour here."
        ),
        wave_controls_
    );
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    controls->addWidget(hint);

    wave_preset_ = new QComboBox(wave_controls_);
    wave_preset_->setObjectName(QStringLiteral("line_wave_preset"));
    wave_preset_->setAccessibleName(str_label("Existing wave profile preset"));
    wave_preset_->addItem(str_label("Custom profile"));
    wave_preset_->addItem(str_label("Muted — thin / short / dry"));
    wave_preset_->addItem(str_label("Balanced — medium / medium / balanced"));
    wave_preset_->addItem(str_label("Resonant — heavy / long / resonant"));
    // Custom is a status, not a preset that overwrites or resets a profile.
    if (auto* model
        = qobject_cast<QStandardItemModel*>(wave_preset_->model())) {
        model->item(0)->setEnabled(false);
    }
    controls->addWidget(wave_preset_);

    wave_preview_ = new line_wave_preview(wave_controls_);
    controls->addWidget(wave_preview_);
    wave_age_label_ = new QLabel(wave_controls_);
    wave_age_label_->setTextFormat(Qt::PlainText);
    controls->addWidget(wave_age_label_);
    wave_age_ = new QSlider(Qt::Horizontal, wave_controls_);
    wave_age_->setObjectName(QStringLiteral("line_wave_age"));
    wave_age_->setAccessibleName(
        str_label("Synthetic hit age in milliseconds")
    );
    wave_age_->setRange(0, int(line_wave_renderer::pulse_ttl_ms));
    wave_age_->setValue(200);
    wave_age_->setSingleStep(20);
    wave_age_->setPageStep(200);
    wave_age_label_->setBuddy(wave_age_);
    controls->addWidget(wave_age_);

    wave_gain_ = new QComboBox(wave_controls_);
    wave_gain_->setObjectName(QStringLiteral("line_wave_emphasis"));
    wave_gain_->setAccessibleName(str_label("Preview displacement emphasis"));
    wave_gain_->addItem(str_label("Preview: Standard (1×)"), 1.0);
    wave_gain_->addItem(str_label("Preview: Strong (2×)"), 2.0);
    wave_gain_->addItem(str_label("Preview: Extreme (3×)"), 3.0);
    wave_gain_->setToolTip(str_label(
        "Preview drawing only. Does not change live pulses, processing, "
        "saved profiles or exported configuration."
    ));
    controls->addWidget(wave_gain_);
    layout->addWidget(wave_controls_);
    wave_controls_->hide();

    connect(wave_checkbox_, &QCheckBox::toggled, this, [this](bool visible) {
        wave_controls_->setVisible(visible);
        if (visible) {
            refresh_wave_controls();
        }
    });
    connect(
        wave_preset_, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (index <= 0 || index > int(std::size(presets))) {
                return;
            }
            const auto& preset = presets[index - 1];
            auto profile = current_line_profile();
            profile.width_text = QString::fromLatin1(preset.width);
            profile.length_text = QString::fromLatin1(preset.span);
            profile.response_text = QString::fromLatin1(preset.response);
            set_line_profile(profile);
            emit profile_changed(current_line_profile());
        }
    );
    connect(wave_age_, &QSlider::valueChanged, this, [this] {
        refresh_wave_controls();
    });
    connect(wave_gain_, &QComboBox::currentIndexChanged, this, [this] {
        refresh_wave_controls();
    });
#endif
}

void line_profile_panel::refresh_wave_controls() {
    if (wave_preview_ == nullptr) {
        return;
    }
    const auto profile = current_line_profile();
    int preset_index = 0;
    for (size_t i = 0; i < std::size(presets); ++i) {
        const auto& preset = presets[i];
        if (profile.width_text == QLatin1String(preset.width)
            && profile.length_text == QLatin1String(preset.span)
            && profile.response_text == QLatin1String(preset.response)) {
            preset_index = int(i) + 1;
            break;
        }
    }
    const QSignalBlocker blocker(wave_preset_);
    wave_preset_->setCurrentIndex(preset_index);
    wave_age_label_->setText(
        str_label("Sample age: %1 ms (no background animation)")
            .arg(wave_age_->value())
    );
    wave_preview_->set_sample(
        profile, wave_age_->value(), wave_gain_->currentData().toDouble()
    );
}
