#ifndef YODAU_FRONTEND_MAIN_WINDOW_HPP
#define YODAU_FRONTEND_MAIN_WINDOW_HPP

#include <QMainWindow>

class main_window : public QMainWindow {
    Q_OBJECT
public:
    explicit main_window(QWidget* parent = nullptr);
    ~main_window() override;
};

#endif // YODAU_FRONTEND_MAIN_WINDOW_HPP