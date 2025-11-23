#include "opencv_cli.hpp"

int yodau::cli::local_index_from_path(const std::string& path) {
    const std::string pref = "/dev/video";
    if (path.rfind(pref, 0) != 0) {
        return -1;
    }
    const auto tail = path.substr(pref.size());
    try {
        return std::stoi(tail);
    } catch (...) {
        return -1;
    }
}

yodau::backend::frame yodau::cli::mat_to_frame(const cv::Mat& m) {
    yodau::backend::frame f;
    f.width = m.cols;
    f.height = m.rows;
    f.stride = static_cast<int>(m.step);
    f.ts = std::chrono::steady_clock::now();

    if (m.channels() == 3 && m.type() == CV_8UC3) {
        f.format = yodau::backend::pixel_format::bgr24;
        f.data.assign(m.data, m.data + m.total() * m.elemSize());
        return f;
    }

    cv::Mat bgr;
    if (m.channels() == 1) {
        cv::cvtColor(m, bgr, cv::COLOR_GRAY2BGR);
    } else {
        m.convertTo(bgr, CV_8UC3);
    }

    f.format = yodau::backend::pixel_format::bgr24;
    f.stride = static_cast<int>(bgr.step);
    f.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
    return f;
}

void yodau::cli::opencv_daemon_start(
    const yodau::backend::stream& s,
    std::function<void(yodau::backend::frame&&)> on_frame, std::stop_token st
) {
    const auto path = s.get_path();
    cv::VideoCapture cap;

    const auto idx = local_index_from_path(path);
    if (idx >= 0) {
        cap.open(idx);
    } else {
        cap.open(path);
    }

    if (!cap.isOpened()) {
        return;
    }

    cv::Mat m;
    while (!st.stop_requested()) {
        if (!cap.read(m) || m.empty()) {
            if (s.is_looping()
                && s.get_type() == yodau::backend::stream_type::file) {
                cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                continue;
            }
            break;
        }

        auto f = mat_to_frame(m);
        on_frame(std::move(f));
    }
}
