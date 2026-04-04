#ifndef YODAU_APP_SHELL_STREAM_CATALOG_STATE_HPP
#define YODAU_APP_SHELL_STREAM_CATALOG_STATE_HPP

#include "shell/app_settings.hpp"

#include <QMap>
#include <QStringList>

#include <string>
#include <vector>

class stream_catalog_state final {
public:
    static stream_settings
    default_stream_settings(const QString& stream_name = QString());
    static stream_settings normalized_stream_settings(
        stream_settings settings_value
    );
    static QStringList detected_local_sources(
        const std::vector<std::string>& core_names
    );

    void ensure_stream(const QString& stream_name);
    void set_stream_settings(stream_settings settings_value);

    stream_settings settings_for(const QString& stream_name) const;
    QString algorithm_id_for(const QString& stream_name) const;

private:
    static QString normalized_stream_name(const QString& stream_name);

    QMap<QString, stream_settings> settings_by_stream_;
};

#endif // YODAU_APP_SHELL_STREAM_CATALOG_STATE_HPP
