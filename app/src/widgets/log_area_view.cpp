#include "widgets/log_area_view.hpp"

#include "widgets/log_toolbar_panel.hpp"

log_area_view::log_area_view(const frontend_log_area area, QWidget* parent)
    : QPlainTextEdit(parent)
    , log_area(area) {
    setReadOnly(true);
}

void log_area_view::set_log_toolbar(log_toolbar_panel* toolbar) {
    if (log_toolbar_widget == toolbar) {
        rebuild_from_toolbar();
        return;
    }

    if (log_toolbar_widget != nullptr) {
        disconnect(log_toolbar_widget, nullptr, this, nullptr);
    }

    log_toolbar_widget = toolbar;

    if (log_toolbar_widget != nullptr) {
        connect(
            log_toolbar_widget, &log_toolbar_panel::view_state_changed, this,
            [this]() { rebuild_from_toolbar(); }
        );
    }

    rebuild_from_toolbar();
}

bool log_area_view::append_entry(const frontend_log_entry& entry) const {
    if (entry.area != log_area) {
        return false;
    }

    if (log_toolbar_widget != nullptr
        && !log_toolbar_widget->entry_matches(entry)) {
        return false;
    }

    const auto self = const_cast<log_area_view*>(this);
    self->appendPlainText(
        format_frontend_log_entry(
            log_toolbar_widget != nullptr ? log_toolbar_widget->log_mode()
                                          : frontend_log_mode::release,
            entry
        )
    );
    return true;
}

void log_area_view::rebuild_from_toolbar() const {
    const auto self = const_cast<log_area_view*>(this);
    self->clear();

    if (log_toolbar_widget == nullptr) {
        return;
    }

    self->setPlainText(
        log_toolbar_widget->formatted_entries(log_area).join('\n')
    );
}
