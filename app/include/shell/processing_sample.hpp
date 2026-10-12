#ifndef YODAU_APP_SHELL_PROCESSING_SAMPLE_HPP
#define YODAU_APP_SHELL_PROCESSING_SAMPLE_HPP

#include "shell/app_settings.hpp"

/// Read-only GUI observations, not a media clock or a pipeline profiler.
struct processing_sample {
    QString stream_name;
    QString source_status;
    QString algorithm_id;
    QString preset_id;
    stream_runtime_metrics metrics;
    qint64 input_age_ms { -1 };
    qint64 result_age_ms { -1 };
    bool application_active { true };
    bool processing_enabled { false };
};

#endif
