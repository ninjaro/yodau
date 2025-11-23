#ifndef YODAU_BACKEND_OPENCV_CLI_HPP
#define YODAU_BACKEND_OPENCV_CLI_HPP

#ifdef YODAU_OPENCV

#include "frame.hpp"
#include "stream.hpp"
#include "stream_manager.hpp"
#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace yodau::cli {

int local_index_from_path(const std::string& path);

backend::frame mat_to_frame(const cv::Mat& m);

void opencv_daemon_start(
    const backend::stream& s, std::function<void(backend::frame&&)> on_frame,
    std::stop_token st
);
}

#endif

#endif // YODAU_BACKEND_OPENCV_CLI_HPP