#include "cli/cli_log.hpp"

#include "core/namespace_alias.hpp"
#include <gtest/gtest.h>

TEST(cli_log_test, release_mode_hides_debug_entries) {
    yodau::core::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::core::cli_log_scope::event;
    entry.severity = yodau::core::cli_log_severity::debug;
    entry.message = "motion detected";

    EXPECT_FALSE(
        yodau::core::cli_log_entry_visible(
            yodau::core::cli_log_mode::release, entry
        )
    );
    EXPECT_TRUE(
        yodau::core::cli_log_entry_visible(
            yodau::core::cli_log_mode::debug, entry
        )
    );
}

TEST(cli_log_test, debug_format_includes_scope_and_detail) {
    yodau::core::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::core::cli_log_scope::event;
    entry.severity = yodau::core::cli_log_severity::warning;
    entry.subsystem = "core_event";
    entry.stream_name = "cam-a";
    entry.line_name = "north";
    entry.algorithm_id = "contour_mask";
    entry.event_type = "tripwire";
    entry.message = "tripwire triggered";
    entry.detail = "pos=(10,20)";

    const std::string release_text = yodau::core::format_cli_log_entry(
        yodau::core::cli_log_mode::release, entry
    );
    const std::string debug_text = yodau::core::format_cli_log_entry(
        yodau::core::cli_log_mode::debug, entry
    );

    EXPECT_NE(release_text.find("warn"), std::string::npos);
    EXPECT_NE(release_text.find("cam-a tripwire"), std::string::npos);
    EXPECT_NE(release_text.find("line=north"), std::string::npos);
    EXPECT_EQ(release_text.find("scope=event"), std::string::npos);
    EXPECT_EQ(release_text.find("alg=contour_mask"), std::string::npos);

    EXPECT_NE(debug_text.find("warn"), std::string::npos);
    EXPECT_NE(debug_text.find("scope=event"), std::string::npos);
    EXPECT_NE(debug_text.find("subsystem=core_event"), std::string::npos);
    EXPECT_NE(debug_text.find("stream=cam-a"), std::string::npos);
    EXPECT_NE(debug_text.find("line=north"), std::string::npos);
    EXPECT_NE(debug_text.find("alg=contour_mask"), std::string::npos);
    EXPECT_NE(debug_text.find("event=tripwire"), std::string::npos);
    EXPECT_NE(debug_text.find("detail=pos=(10,20)"), std::string::npos);
}

TEST(cli_log_test, filters_severity_stream_line_event_algorithm_and_subsystem) {
    yodau::core::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::core::cli_log_scope::command;
    entry.severity = yodau::core::cli_log_severity::error;
    entry.subsystem = "stream_add";
    entry.stream_name = "cam-a";
    entry.line_name = "north";
    entry.algorithm_id = "spot_grid";
    entry.event_type = "info";
    entry.message = "stream add failed";

    yodau::core::cli_log_filter filter;
    filter.severity = yodau::core::cli_log_severity::error;
    filter.stream_name = "cam-a";
    filter.line_name = "north";
    filter.algorithm_id = "spot_grid";
    filter.event_type = "info";
    filter.subsystem = "stream_add";

    EXPECT_TRUE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );

    filter.stream_name = "cam-b";
    EXPECT_FALSE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );

    filter.stream_name = "cam-a";
    filter.severity = yodau::core::cli_log_severity::warning;
    EXPECT_FALSE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );

    filter.severity = yodau::core::cli_log_severity::error;
    filter.line_name = "south";
    EXPECT_FALSE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );

    filter.line_name = "north";
    filter.algorithm_id = "motion_baseline";
    EXPECT_FALSE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );

    filter.algorithm_id = "spot_grid";
    filter.event_type = "tripwire";
    EXPECT_FALSE(
        yodau::core::cli_log_entry_matches(
            yodau::core::cli_log_mode::release, entry, filter
        )
    );
}
