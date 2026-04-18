#ifndef YODAU_APP_SHELL_APP_SETTINGS_HPP
#define YODAU_APP_SHELL_APP_SETTINGS_HPP

#include <QColor>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QStringList>

QString default_line_color_mode_id();
QStringList line_color_mode_ids();
QString line_color_mode_display_name(const QString& mode_id);
QString normalized_line_color_mode_id(const QString& mode_id);
QColor random_manual_line_color();
QColor auto_palette_line_color(int line_index, int line_count);
QColor softened_negative_line_color(const QColor& sampled_color);

QString default_line_width_text();
QString default_line_length_text();
QString default_line_response_text();
int default_manual_display_fps();
int default_manual_core_fps();
int default_manual_processing_pixels();
QString default_line_parameter_mode_id();
QStringList line_parameter_mode_ids();
QString line_parameter_mode_display_name(const QString& mode_id);
QString normalized_line_parameter_mode_id(const QString& mode_id);
QString line_width_label_text(const QString& mode_id);
QString line_length_label_text(const QString& mode_id);
QString line_response_label_text(const QString& mode_id);
QString line_parameter_mode_hint_text(const QString& mode_id);
QString default_movement_display_mode_id();
QStringList movement_display_mode_ids();
QString movement_display_mode_display_name(const QString& mode_id);
QString normalized_movement_display_mode_id(const QString& mode_id);
bool movement_display_enabled(const QString& mode_id);

struct stream_settings {
    QString stream_name;
    bool labels_enabled { true };
    bool standard_labels_enabled { true };
    QString algorithm_id;
    QString algorithm_preset;
    QString movement_display_mode { default_movement_display_mode_id() };
    bool algorithm_overlay_enabled { true };
    bool manual_processing_policy_enabled { false };
    int manual_display_fps { default_manual_display_fps() };
    int manual_core_fps { default_manual_core_fps() };
    int manual_processing_pixels { default_manual_processing_pixels() };
};

Q_DECLARE_METATYPE(stream_settings)

struct stream_runtime_metrics {
    double input_fps { 0.0 };
    double core_fps { 0.0 };
    int input_width { 0 };
    int input_height { 0 };
    int processed_width { 0 };
    int processed_height { 0 };
    int effective_display_fps { 0 };
    int effective_core_fps { 0 };
    int effective_processing_pixels { 0 };
    bool manual_policy_active { false };
    QString processing_summary;
};

Q_DECLARE_METATYPE(stream_runtime_metrics)

struct line_profile {
    QString name;
    QColor color { Qt::red };
    QString color_mode_id { default_line_color_mode_id() };
    bool closed { false };
    QString width_text { default_line_width_text() };
    QString length_text { default_line_length_text() };
    QString response_text { default_line_response_text() };
};

Q_DECLARE_METATYPE(line_profile)

struct template_apply_settings {
    QString template_name;
    QColor color { Qt::red };
    QString color_mode_id { default_line_color_mode_id() };
    QString width_text { default_line_width_text() };
    QString length_text { default_line_length_text() };
    QString response_text { default_line_response_text() };
};

Q_DECLARE_METATYPE(template_apply_settings)

struct line_edit_request {
    QString stream_name;
    QString source_line_name;
    line_profile profile;
    QVector<QPointF> points_pct;
};

Q_DECLARE_METATYPE(line_edit_request)

QString default_app_algorithm_id();
QStringList app_algorithm_ids();
QString app_algorithm_display_name(const QString& algorithm_id);
QString normalized_app_algorithm_id(const QString& algorithm_id);
QString default_operator_profile_id();
QStringList operator_profile_ids(bool include_custom = false);
QString operator_profile_display_name(const QString& profile_id);
QString normalized_operator_profile_id(const QString& profile_id);
stream_settings apply_operator_profile(
    stream_settings settings_value, const QString& profile_id
);
QString inferred_operator_profile_id(const stream_settings& settings_value);
QString operator_profile_summary_text(const stream_settings& settings_value);
QString default_algorithm_preset_id(const QString& algorithm_id);
QStringList algorithm_preset_ids(const QString& algorithm_id);
QString algorithm_preset_display_name(
    const QString& algorithm_id, const QString& preset_id
);
QString normalized_algorithm_preset_id(
    const QString& algorithm_id, const QString& preset_id
);
QString algorithm_summary_text(
    const QString& algorithm_id, const QString& preset_id,
    const QString& movement_display_mode
);
QString algorithm_badge_text(
    const QString& algorithm_id, const QString& preset_id,
    bool overlay_enabled
);
QColor algorithm_badge_color(const QString& algorithm_id);

QStringList suggested_line_width_texts();
QString normalized_line_width_text(const QString& width_text);
qreal line_width_visual_value(const QString& width_text);
QStringList suggested_line_length_texts();
QString normalized_line_length_text(const QString& length_text);
QStringList suggested_line_response_texts();
QString normalized_line_response_text(const QString& response_text);
QString line_profile_summary_text(
    const QString& width_text, const QString& length_text,
    const QString& response_text,
    const QString& parameter_mode_id = default_line_parameter_mode_id()
);
QString stream_processing_policy_summary_text(
    const stream_settings& settings_value
);
QString stream_runtime_metrics_text(const stream_runtime_metrics& metrics);

#endif // YODAU_APP_SHELL_APP_SETTINGS_HPP
