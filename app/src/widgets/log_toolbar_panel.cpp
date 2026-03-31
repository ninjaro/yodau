#include "widgets/log_toolbar_panel.hpp"

#include "shell/str_label.hpp"

#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTextStream>

#include <algorithm>

namespace log_toolbar_panel_support {

QString filter_all_text() { return str_label("all"); }

QString log_area_title(const frontend_log_area area) {
    switch (area) {
    case frontend_log_area::add:
        return QStringLiteral("add");
    case frontend_log_area::streams:
        return QStringLiteral("streams");
    case frontend_log_area::active:
        return QStringLiteral("active");
    }

    return QStringLiteral("active");
}

void replace_filter_combo_items(
    QComboBox* combo, const QStringList& values, const QString& selected_value
) {
    if (combo == nullptr) {
        return;
    }

    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(filter_all_text(), QString());

    for (const QString& value : values) {
        combo->addItem(value, value);
    }

    const int selected_index
        = selected_value.isEmpty() ? 0 : combo->findData(selected_value);
    combo->setCurrentIndex(selected_index >= 0 ? selected_index : 0);
}

} // namespace log_toolbar_panel_support

log_toolbar_panel::log_toolbar_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    refresh_filter_options();
}

void log_toolbar_panel::set_log_buffer(frontend_log_buffer* buffer) {
    if (shared_log_buffer == buffer) {
        refresh_filter_options();
        emit view_state_changed();
        return;
    }

    if (shared_log_buffer != nullptr) {
        disconnect(shared_log_buffer, nullptr, this, nullptr);
    }

    shared_log_buffer = buffer;

    if (shared_log_buffer != nullptr) {
        connect(
            shared_log_buffer, &frontend_log_buffer::entry_appended, this,
            [this](const frontend_log_entry&) {
                refresh_filter_options();
                emit view_state_changed();
            }
        );
        connect(
            shared_log_buffer, &frontend_log_buffer::cleared, this,
            [this]() {
                refresh_filter_options();
                emit view_state_changed();
            }
        );
    }

    refresh_filter_options();
    emit view_state_changed();
}

bool log_toolbar_panel::append_entry(frontend_log_entry entry) const {
    if (shared_log_buffer == nullptr) {
        return false;
    }

    shared_log_buffer->append(entry);
    return true;
}

void log_toolbar_panel::set_log_mode(const frontend_log_mode mode) {
    current_log_mode = mode;

    if (log_mode_combo != nullptr) {
        QSignalBlocker blocker(log_mode_combo);
        log_mode_combo->setCurrentIndex(
            mode == frontend_log_mode::release ? 0 : 1
        );
    }

    emit view_state_changed();
}

frontend_log_mode log_toolbar_panel::log_mode() const {
    return current_log_mode;
}

bool log_toolbar_panel::entry_matches(const frontend_log_entry& entry) const {
    const int active_severity_filter = log_severity_filter_combo != nullptr
        ? log_severity_filter_combo->currentData().toInt()
        : current_log_severity_filter;
    const QString active_stream_filter = log_stream_filter_combo != nullptr
        ? log_stream_filter_combo->currentData().toString()
        : current_log_stream_filter;
    const QString active_subsystem_filter
        = log_subsystem_filter_combo != nullptr
        ? log_subsystem_filter_combo->currentData().toString()
        : current_log_subsystem_filter;

    if (current_log_mode == frontend_log_mode::release
        && entry.severity == frontend_log_severity::debug) {
        return false;
    }

    if (active_severity_filter >= 0
        && static_cast<int>(entry.severity) != active_severity_filter) {
        return false;
    }

    if (!active_stream_filter.isEmpty()
        && entry.stream_name != active_stream_filter) {
        return false;
    }

    if (!active_subsystem_filter.isEmpty()
        && entry.subsystem != active_subsystem_filter) {
        return false;
    }

    return true;
}

QStringList log_toolbar_panel::formatted_entries(
    const frontend_log_area area
) const {
    QStringList lines;
    if (shared_log_buffer == nullptr) {
        return lines;
    }

    for (const frontend_log_entry& entry :
         shared_log_buffer->entries_for_area(area)) {
        if (entry_matches(entry)) {
            lines.push_back(format_frontend_log_entry(current_log_mode, entry));
        }
    }

    return lines;
}

QString log_toolbar_panel::compose_log_report(
    const frontend_log_area area
) const {
    QStringList lines;
    lines << QStringLiteral("yodau log report");
    lines << QStringLiteral("area=%1 mode=%2 severity=%3 stream=%4 subsystem=%5")
                 .arg(log_toolbar_panel_support::log_area_title(area))
                 .arg(
                     current_log_mode == frontend_log_mode::release
                         ? QStringLiteral("release")
                         : QStringLiteral("debug")
                 )
                 .arg(
                     current_log_severity_filter < 0
                         ? log_toolbar_panel_support::filter_all_text()
                         : frontend_log_severity_name(
                               static_cast<frontend_log_severity>(
                                   current_log_severity_filter
                               )
                           )
                 )
                 .arg(
                     current_log_stream_filter.isEmpty()
                         ? log_toolbar_panel_support::filter_all_text()
                         : current_log_stream_filter
                 )
                 .arg(
                     current_log_subsystem_filter.isEmpty()
                         ? log_toolbar_panel_support::filter_all_text()
                         : current_log_subsystem_filter
                 );

    const QStringList visible_entries = formatted_entries(area);
    lines.append(visible_entries);

    if (visible_entries.isEmpty()) {
        lines << QStringLiteral("(no matching entries)");
    }

    return lines.join('\n');
}

QString log_toolbar_panel::compose_log_summary(
    const frontend_log_area area
) const {
    int debug_count = 0;
    int info_count = 0;
    int warning_count = 0;
    int error_count = 0;
    QSet<QString> visible_streams;
    QSet<QString> visible_subsystems;

    if (shared_log_buffer != nullptr) {
        for (const frontend_log_entry& entry :
             shared_log_buffer->entries_for_area(area)) {
            if (!entry_matches(entry)) {
                continue;
            }

            switch (entry.severity) {
            case frontend_log_severity::debug:
                debug_count += 1;
                break;
            case frontend_log_severity::info:
                info_count += 1;
                break;
            case frontend_log_severity::warning:
                warning_count += 1;
                break;
            case frontend_log_severity::error:
                error_count += 1;
                break;
            }

            if (!entry.stream_name.trimmed().isEmpty()) {
                visible_streams.insert(entry.stream_name.trimmed());
            }
            if (!entry.subsystem.trimmed().isEmpty()) {
                visible_subsystems.insert(entry.subsystem.trimmed());
            }
        }
    }

    QStringList lines;
    lines << QStringLiteral("yodau log summary");
    lines << QStringLiteral("area=%1").arg(
        log_toolbar_panel_support::log_area_title(area)
    );
    lines << QStringLiteral("entries=%1 debug=%2 info=%3 warn=%4 error=%5")
                 .arg(
                     debug_count + info_count + warning_count + error_count
                 )
                 .arg(debug_count)
                 .arg(info_count)
                 .arg(warning_count)
                 .arg(error_count);
    lines << QStringLiteral("streams=%1 subsystems=%2")
                 .arg(visible_streams.size())
                 .arg(visible_subsystems.size());

    QStringList stream_list = visible_streams.values();
    QStringList subsystem_list = visible_subsystems.values();
    std::sort(
        stream_list.begin(), stream_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        subsystem_list.begin(), subsystem_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );

    if (!visible_streams.isEmpty()) {
        lines << QStringLiteral("stream_list=%1")
                     .arg(stream_list.join(','));
    }
    if (!visible_subsystems.isEmpty()) {
        lines << QStringLiteral("subsystem_list=%1")
                     .arg(subsystem_list.join(','));
    }

    return lines.join('\n');
}

bool log_toolbar_panel::write_log_report(
    const frontend_log_area area, const QString& path
) const {
    if (path.trimmed().isEmpty()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << compose_log_report(area);
    return stream.status() == QTextStream::Ok;
}

void log_toolbar_panel::on_log_mode_changed(const int index) {
    set_log_mode(
        index == 0 ? frontend_log_mode::release : frontend_log_mode::debug
    );
}

void log_toolbar_panel::on_log_severity_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_severity_filter_combo == nullptr) {
        return;
    }

    current_log_severity_filter
        = log_severity_filter_combo->currentData().toInt();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_stream_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_stream_filter_combo == nullptr) {
        return;
    }

    current_log_stream_filter = log_stream_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_subsystem_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_subsystem_filter_combo == nullptr) {
        return;
    }

    current_log_subsystem_filter
        = log_subsystem_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::build_ui() {
    const auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto label = new QLabel(str_label("log mode"), this);
    log_mode_combo = new QComboBox(this);
    log_mode_combo->setObjectName(QStringLiteral("settings_log_mode_combo"));
    log_mode_combo->addItem(str_label("release"));
    log_mode_combo->addItem(str_label("debug"));
    log_mode_combo->setCurrentIndex(
        current_log_mode == frontend_log_mode::release ? 0 : 1
    );

    const auto severity_label = new QLabel(str_label("severity"), this);
    log_severity_filter_combo = new QComboBox(this);
    log_severity_filter_combo->setObjectName(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    log_severity_filter_combo->addItem(
        log_toolbar_panel_support::filter_all_text(), -1
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::debug),
        static_cast<int>(frontend_log_severity::debug)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::info),
        static_cast<int>(frontend_log_severity::info)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::warning),
        static_cast<int>(frontend_log_severity::warning)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::error),
        static_cast<int>(frontend_log_severity::error)
    );

    const auto stream_label = new QLabel(str_label("stream"), this);
    log_stream_filter_combo = new QComboBox(this);
    log_stream_filter_combo->setObjectName(
        QStringLiteral("settings_log_stream_filter_combo")
    );

    const auto subsystem_label = new QLabel(str_label("subsystem"), this);
    log_subsystem_filter_combo = new QComboBox(this);
    log_subsystem_filter_combo->setObjectName(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );

    copy_logs_btn = new QPushButton(str_label("copy logs"), this);
    copy_logs_btn->setObjectName(QStringLiteral("settings_copy_logs_button"));
    copy_summary_btn = new QPushButton(str_label("copy summary"), this);
    copy_summary_btn->setObjectName(
        QStringLiteral("settings_copy_summary_button")
    );
    save_logs_btn = new QPushButton(str_label("save logs"), this);
    save_logs_btn->setObjectName(QStringLiteral("settings_save_logs_button"));

    layout->addWidget(label);
    layout->addWidget(log_mode_combo);
    layout->addWidget(severity_label);
    layout->addWidget(log_severity_filter_combo);
    layout->addWidget(stream_label);
    layout->addWidget(log_stream_filter_combo);
    layout->addWidget(subsystem_label);
    layout->addWidget(log_subsystem_filter_combo);
    layout->addWidget(copy_logs_btn);
    layout->addWidget(copy_summary_btn);
    layout->addWidget(save_logs_btn);
    layout->addStretch(1);

    connect(
        log_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &log_toolbar_panel::on_log_mode_changed
    );
    connect(
        log_severity_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_severity_filter_changed
    );
    connect(
        log_stream_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_stream_filter_changed
    );
    connect(
        log_subsystem_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_subsystem_filter_changed
    );
    connect(
        copy_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_logs_requested
    );
    connect(
        copy_summary_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_summary_requested
    );
    connect(
        save_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::save_logs_requested
    );
}

void log_toolbar_panel::refresh_filter_options() const {
    QStringList stream_names;
    QStringList subsystem_names;

    if (shared_log_buffer != nullptr) {
        for (const frontend_log_entry& entry : shared_log_buffer->entries()) {
            const QString stream_name = entry.stream_name.trimmed();
            if (!stream_name.isEmpty() && !stream_names.contains(stream_name)) {
                stream_names.push_back(stream_name);
            }

            const QString subsystem_name = entry.subsystem.trimmed();
            if (!subsystem_name.isEmpty()
                && !subsystem_names.contains(subsystem_name)) {
                subsystem_names.push_back(subsystem_name);
            }
        }
    }

    std::sort(
        stream_names.begin(), stream_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        subsystem_names.begin(), subsystem_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );

    log_toolbar_panel_support::replace_filter_combo_items(
        log_stream_filter_combo, stream_names, current_log_stream_filter
    );
    log_toolbar_panel_support::replace_filter_combo_items(
        log_subsystem_filter_combo, subsystem_names,
        current_log_subsystem_filter
    );
}
