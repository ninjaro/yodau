#include "cli/cli_log.hpp"

#include <gtest/gtest.h>

TEST(cli_log_test, release_mode_hides_debug_entries) {
    yodau::backend::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::backend::cli_log_scope::event;
    entry.severity = yodau::backend::cli_log_severity::debug;
    entry.message = "motion detected";

    EXPECT_FALSE(
        yodau::backend::cli_log_entry_visible(
            yodau::backend::cli_log_mode::release, entry
        )
    );
    EXPECT_TRUE(
        yodau::backend::cli_log_entry_visible(
            yodau::backend::cli_log_mode::debug, entry
        )
    );
}

TEST(cli_log_test, debug_format_includes_scope_and_detail) {
    yodau::backend::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::backend::cli_log_scope::event;
    entry.severity = yodau::backend::cli_log_severity::warning;
    entry.subsystem = "backend_event";
    entry.stream_name = "cam-a";
    entry.message = "tripwire triggered";
    entry.detail = "line=A";

    const std::string release_text = yodau::backend::format_cli_log_entry(
        yodau::backend::cli_log_mode::release, entry
    );
    const std::string debug_text = yodau::backend::format_cli_log_entry(
        yodau::backend::cli_log_mode::debug, entry
    );

    EXPECT_NE(release_text.find("warn"), std::string::npos);
    EXPECT_NE(release_text.find("cam-a tripwire triggered"), std::string::npos);
    EXPECT_EQ(release_text.find("scope=event"), std::string::npos);

    EXPECT_NE(debug_text.find("warn"), std::string::npos);
    EXPECT_NE(debug_text.find("scope=event"), std::string::npos);
    EXPECT_NE(debug_text.find("subsystem=backend_event"), std::string::npos);
    EXPECT_NE(debug_text.find("stream=cam-a"), std::string::npos);
    EXPECT_NE(debug_text.find("detail=line=A"), std::string::npos);
}

TEST(cli_log_test, filters_match_severity_stream_and_subsystem) {
    yodau::backend::cli_log_entry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.scope = yodau::backend::cli_log_scope::command;
    entry.severity = yodau::backend::cli_log_severity::error;
    entry.subsystem = "stream_add";
    entry.stream_name = "cam-a";
    entry.message = "stream add failed";

    yodau::backend::cli_log_filter filter;
    filter.severity = yodau::backend::cli_log_severity::error;
    filter.stream_name = "cam-a";
    filter.subsystem = "stream_add";

    EXPECT_TRUE(
        yodau::backend::cli_log_entry_matches(
            yodau::backend::cli_log_mode::release, entry, filter
        )
    );

    filter.stream_name = "cam-b";
    EXPECT_FALSE(
        yodau::backend::cli_log_entry_matches(
            yodau::backend::cli_log_mode::release, entry, filter
        )
    );

    filter.stream_name = "cam-a";
    filter.severity = yodau::backend::cli_log_severity::warning;
    EXPECT_FALSE(
        yodau::backend::cli_log_entry_matches(
            yodau::backend::cli_log_mode::release, entry, filter
        )
    );
}
