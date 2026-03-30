#ifndef YODAU_FRONTEND_SHELL_FRONTEND_SETTINGS_HPP
#define YODAU_FRONTEND_SHELL_FRONTEND_SETTINGS_HPP

#include <QColor>
#include <QMetaType>
#include <QString>
#include <QStringList>

QString default_line_width_text();
QString default_line_length_text();
QString default_line_response_text();

struct stream_settings {
    QString stream_name;
    bool labels_enabled { true };
    QString algorithm_id;
    QString algorithm_preset;
    bool algorithm_overlay_enabled { false };
};

Q_DECLARE_METATYPE(stream_settings)

struct line_profile {
    QString name;
    QColor color { Qt::red };
    bool closed { false };
    QString width_text { default_line_width_text() };
    QString length_text { default_line_length_text() };
    QString response_text { default_line_response_text() };
};

Q_DECLARE_METATYPE(line_profile)

struct template_apply_settings {
    QString template_name;
    QColor color { Qt::red };
    QString width_text { default_line_width_text() };
    QString length_text { default_line_length_text() };
    QString response_text { default_line_response_text() };
};

Q_DECLARE_METATYPE(template_apply_settings)

QString default_frontend_algorithm_id();
QStringList frontend_algorithm_ids();
QString frontend_algorithm_display_name(const QString& algorithm_id);
QString normalized_frontend_algorithm_id(const QString& algorithm_id);
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
    bool overlay_enabled
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
    const QString& response_text
);

#endif // YODAU_FRONTEND_SHELL_FRONTEND_SETTINGS_HPP
