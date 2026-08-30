#ifndef YODAU_APP_DATA_MONITOR_TELEMETRY_JSON_HPP
#define YODAU_APP_DATA_MONITOR_TELEMETRY_JSON_HPP

#include "debug/telemetry_records.hpp"

#include <QByteArray>

namespace yodau::data {

[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::hello_record& record);
[[nodiscard]] QByteArray encode_telemetry_json(
    const yodau::observability::sample_batch_record& record
);
[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::event_batch_record& record);
[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::snapshot_record& record);
[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::marker_record& record);
[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::warning_record& record);
[[nodiscard]] QByteArray
encode_telemetry_json(const yodau::observability::goodbye_record& record);

} // namespace yodau::data

#endif // YODAU_APP_DATA_MONITOR_TELEMETRY_JSON_HPP
