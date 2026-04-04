#ifndef YODAU_APP_SHELL_FRONTEND_LOG_HPP
#define YODAU_APP_SHELL_FRONTEND_LOG_HPP

#include "shell/app_log.hpp"

using frontend_log_area = app_log_area;
using frontend_log_severity = app_log_severity;
using frontend_log_mode = app_log_mode;
using frontend_log_entry = app_log_entry;
using frontend_log_buffer = app_log_buffer;

#define frontend_log_area_name app_log_area_name
#define frontend_log_severity_name app_log_severity_name
#define frontend_log_mode_name app_log_mode_name
#define format_frontend_log_entry format_app_log_entry

#endif // YODAU_APP_SHELL_FRONTEND_LOG_HPP
