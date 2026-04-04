#ifndef YODAU_APP_SHELL_ACTIVE_EDIT_SESSION_HPP
#define YODAU_APP_SHELL_ACTIVE_EDIT_SESSION_HPP

#include "shell/app_settings.hpp"
#include "widgets/stream_cell.hpp"

#include <QMap>
#include <QPointF>
#include <QSet>
#include <QStringList>

#include <optional>
#include <vector>

class active_edit_session final {
public:
    struct template_line {
        std::vector<QPointF> pts_pct;
        bool closed { false };
        QString width_text { default_line_width_text() };
        QString length_text { default_line_length_text() };
        QString response_text { default_line_response_text() };
    };

    active_edit_session();

    bool drawing_new_mode() const;
    void set_drawing_new_mode(bool drawing_new);

    const line_profile& draft_line_profile() const;
    void set_draft_line_profile(line_profile profile_value);
    void reset_draft_line_profile();

    const template_apply_settings& active_template_settings() const;
    void set_active_template_settings(template_apply_settings settings_value);
    template_apply_settings resolved_template_settings(
        template_apply_settings settings_value, bool inherit_template_profile
    ) const;
    void reset_active_template_settings();

    bool has_template(const QString& template_name) const;
    std::optional<template_line> template_value(const QString& template_name) const;

    stream_cell::line_instance store_saved_line(
        const QString& stream_name, const QString& final_name,
        const std::vector<QPointF>& pts, bool closed
    );
    stream_cell::line_instance store_applied_template_line(
        const QString& stream_name, const template_apply_settings& settings_value
    );
    bool set_stream_line_enabled(
        const QString& stream_name, const QString& line_name, bool enabled
    );
    bool detach_stream_line(
        const QString& stream_name, const QString& line_name
    );
    std::optional<stream_cell::line_instance> find_stream_line(
        const QString& stream_name, const QString& line_name
    ) const;
    stream_cell::line_instance store_stream_line(
        const QString& stream_name, stream_cell::line_instance line_value
    );

    void set_active_line_edit(line_edit_request request);
    void clear_active_line_edit();
    std::optional<line_edit_request> active_line_edit() const;

    const std::vector<stream_cell::line_instance>&
    stream_lines(const QString& stream_name) const;
    QSet<QString> used_template_names_for_stream(const QString& stream_name) const;
    QStringList template_candidates_excluding(const QSet<QString>& used) const;

private:
    static line_profile normalized_line_profile(line_profile profile_value);
    static template_apply_settings normalized_template_settings(
        template_apply_settings settings_value
    );
    static line_edit_request normalized_line_edit_request(
        line_edit_request request
    );

    bool drawing_new_mode_ { true };
    line_profile draft_line_profile_;
    template_apply_settings active_template_settings_;
    std::optional<line_edit_request> active_line_edit_;
    QMap<QString, template_line> templates_;
    QMap<QString, std::vector<stream_cell::line_instance>> per_stream_lines_;
};

#endif // YODAU_APP_SHELL_ACTIVE_EDIT_SESSION_HPP
