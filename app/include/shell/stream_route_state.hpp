#ifndef YODAU_FRONTEND_SHELL_STREAM_ROUTE_STATE_HPP
#define YODAU_FRONTEND_SHELL_STREAM_ROUTE_STATE_HPP

#include <QString>

class stream_route_state final {
public:
    struct add_source_validation {
        bool valid { true };
        QString message;
        QString detail;
    };

    static add_source_validation
    validate_add_source(const QString& source, const QString& type);
    static QString source_description(
        const QString& source, const QString& type
    );

    bool has_active_stream() const;
    const QString& active_stream_name() const;
    bool is_active_stream(const QString& stream_name) const;

    void set_active_stream(const QString& stream_name);
    void clear_active_stream();
    bool hide_stream(const QString& stream_name);
    QString next_active_stream_for_enlarge(const QString& stream_name) const;

private:
    static QString normalized_stream_name(const QString& stream_name);

    QString active_stream_name_;
};

#endif // YODAU_FRONTEND_SHELL_STREAM_ROUTE_STATE_HPP
