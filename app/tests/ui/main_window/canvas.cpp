#include "ui/main_window_tests.hpp"

#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_stream_state.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QtTest/QtTest>

namespace canvas_tests {

void activate(QComboBox* combo, int index) {
    combo->setCurrentIndex(index);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index)));
}

struct fixture {
    settings_panel settings;
    stream_board board;
    active_edit_session session;
    stream_widget_bridge bridge { &board, &settings };
    active_edit_controller controller { session, bridge };
    stream_catalog_state catalog;
    stream_route_state route;
    active_stream_state streams { catalog, route, bridge, session };
    active_editor_panel* editor = settings.findChild<active_editor_panel*>();
    QWidget* tools = editor->make_canvas_tools(&board);

    fixture() {
        qRegisterMetaType<line_edit_request>();
        qRegisterMetaType<line_profile>();
        board.add_canvas_tools(tools);
        for (const auto& name : { "one", "two" }) {
            board.grid_mode()->add_stream(QString::fromLatin1(name));
            catalog.ensure_stream(QString::fromLatin1(name));
        }
        settings.set_active_candidates({ "one", "two" });
        for (const auto& name : { "north", "south", "disabled" }) {
            session.store_stream_line(
                "one",
                stream_cell::line_instance {
                    .template_name = QString::fromLatin1(name),
                    .color = QColor(Qt::green),
                    .enabled = QString::fromLatin1(name) != "disabled",
                    .closed = true,
                    .width_text = "string_heavy",
                    .length_text = "long",
                    .response_text = "resonant",
                    .pts_pct = { { 10, 20 }, { 80, 70 } },
                }
            );
        }
        session.store_stream_line(
            "two",
            stream_cell::line_instance {
                .template_name = "north",
                .pts_pct = { { 20, 40 }, { 70, 80 } },
            }
        );
        QObject::connect(
            &settings, &settings_panel::active_edit_mode_changed, &settings,
            [this](bool drawing) { controller.set_drawing_new_mode(drawing); }
        );
        QObject::connect(
            &settings, &settings_panel::active_line_profile_changed, &settings,
            [this](line_profile profile) {
                static_cast<void>(controller.apply_line_profile(profile));
            }
        );
        QObject::connect(
            &settings, &settings_panel::active_line_edit_preview_changed,
            &settings, [this](line_edit_request request) {
                static_cast<void>(controller.apply_line_edit_preview(request));
            }
        );
        QObject::connect(
            &settings, &settings_panel::active_line_edit_preview_cleared,
            &settings, [this] { controller.clear_line_edit_preview(); }
        );
        static_cast<void>(streams.set_active_stream("one"));
        board.resize(640, 420);
        board.show();
        if (tools != nullptr) {
            tools->show();
        }
        QCoreApplication::processEvents();
    }

    template <class T> T* control(const char* name) {
        return tools->findChild<T*>(QString::fromLatin1(name));
    }
};

} // namespace canvas_tests

void main_window_tests::canvas_tools_controls() {
    canvas_tests::fixture f;
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(f.tools == nullptr);
    QVERIFY(f.board.findChild<QWidget*>("canvas_tools") == nullptr);
#else
    QVERIFY(f.tools != nullptr);
    QCOMPARE(f.editor->make_canvas_tools(&f.board), f.tools);
    auto* tool = f.control<QComboBox>("canvas_tool");
    auto* target = f.control<QComboBox>("canvas_target");
    auto* name = f.control<QLineEdit>("canvas_name");
    auto* save = f.control<QPushButton>("canvas_save");
    QVERIFY(tool && target && name && save);
    QVERIFY(!tool->accessibleName().isEmpty());
    QVERIFY(!target->accessibleName().isEmpty());
    QVERIFY(!name->accessibleName().isEmpty());
    QCOMPARE(tool->currentIndex(), 1);
    QVERIFY(target->isHidden());
    // Real keyboard activation, without needing the hidden Lines dock.
    tool->setFocus();
    QTest::keyClick(tool, Qt::Key_Down);
    QCOMPARE(tool->currentIndex(), 2);
    QVERIFY(!target->isHidden());
    QVERIFY(!save->isEnabled());
    QCOMPARE(target->count(), 3); // Disabled geometry is not offered.
    QVERIFY(target->findData("disabled") < 0);
    canvas_tests::activate(target, target->findData("north"));
    QVERIFY(save->isEnabled());
    QCOMPARE(name->text(), QStringLiteral("north_edit"));
    QVERIFY(f.editor->move_line_edit_point(1, { 65, 50 }));
    bool asked = false;
    QTimer::singleShot(0, &f.board, [&] {
        auto* dialog
            = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            asked = true;
            dialog->button(QMessageBox::Discard)->click();
        }
    });
    canvas_tests::activate(target, target->findData("south"));
    QVERIFY(asked);
    QCOMPARE(
        f.session.active_line_edit()->source_line_name, QStringLiteral("south")
    );
    QCOMPARE(f.session.active_line_edit()->points_pct[1], QPointF(80, 70));
    QVERIFY(!f.control<QPushButton>("canvas_undo")->isEnabled());
    canvas_tests::activate(target, 0);
    QCOMPARE(target->currentIndex(), 0);
    QVERIFY(!save->isEnabled());
    auto* tabs
        = f.editor->findChild<QTabWidget*>("settings_active_editor_tabs");
    tabs->setCurrentIndex(0); // Old dock controls update the strip too.
    QCOMPARE(tool->currentIndex(), 1);
    f.settings.set_active_line_profile(
        line_profile { .name = "from dock", .closed = true }
    );
    QCOMPARE(name->text(), QStringLiteral("from dock"));
    QVERIFY(f.control<QCheckBox>("canvas_closed")->isChecked());
    name->setFocus();
    name->selectAll();
    QTest::keyClicks(name, "new geometry");
    QCOMPARE(name->text(), QStringLiteral("new geometry"));
    QCOMPARE(
        f.settings.current_active_line_profile().name,
        QStringLiteral("new geometry")
    );
    QTest::keyClick(name, Qt::Key_Home);
    QTest::keyClicks(
        name, "a "
    ); // Preserve cursor and spaces during mirroring.
    QCOMPARE(name->text(), QStringLiteral("a new geometry"));
    f.tools->hide();
    QVERIFY(f.tools->isHidden());
    QVERIFY(f.board.active_cell() != nullptr);
#endif
}

void main_window_tests::canvas_tools_draw() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop tools only");
#else
    canvas_tests::fixture f;
    auto* cell = f.board.active_cell();
    auto* tool = f.control<QComboBox>("canvas_tool");
    QTest::keyClick(cell, Qt::Key_Space);
    QTest::keyClick(cell, Qt::Key_Right);
    QTest::keyClick(cell, Qt::Key_Space);
    const auto draft = cell->draft_points_pct();
    QCOMPARE(draft.size(), std::size_t(2));
    canvas_tests::activate(tool, 2);
    auto* target = f.control<QComboBox>("canvas_target");
    canvas_tests::activate(target, target->findData("north"));
    QVERIFY(cell->has_line_edit_preview());
    QCOMPARE(cell->draft_points_pct(), draft);
    canvas_tests::activate(tool, 1);
    QVERIFY(!cell->has_line_edit_preview());
    QVERIFY(cell->is_drawing_enabled());
    QCOMPARE(cell->draft_points_pct(), draft);
    QSignalSpy saved(&f.settings, &settings_panel::active_line_save_requested);
    f.control<QCheckBox>("canvas_closed")->click();
    f.control<QPushButton>("canvas_save")->click();
    QCOMPARE(saved.count(), 1);
    QVERIFY(qvariant_cast<line_profile>(saved.last().at(0)).closed);
    for (const auto answer : { QMessageBox::Cancel, QMessageBox::Discard }) {
        bool asked = false;
        QTimer::singleShot(0, &f.board, [&] {
            auto* dialog
                = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (dialog != nullptr) {
                asked = true;
                dialog->button(answer)->click();
            }
        });
        canvas_tests::activate(tool, 0);
        QVERIFY(asked);
        if (answer == QMessageBox::Cancel) {
            QCOMPARE(tool->currentIndex(), 1);
            QCOMPARE(cell->draft_points_pct(), draft);
        }
    }
    QCOMPARE(tool->currentIndex(), 0);
    QVERIFY(!cell->is_drawing_enabled());
    QVERIFY(cell->draft_points_pct().empty());
    QTest::mouseClick(
        cell, Qt::LeftButton, Qt::NoModifier, cell->rect().center()
    );
    QVERIFY(cell->draft_points_pct().empty());
#endif
}

void main_window_tests::canvas_tools_edits_data() {
    QTest::addColumn<int>("turns");
    QTest::addColumn<bool>("fit");
    QTest::addColumn<bool>("contextual");
    for (int turns = 0; turns < 4; ++turns) {
        for (bool fit : { false, true }) {
            for (bool contextual : { false, true }) {
                const auto name = QStringLiteral("%1-%2-%3")
                                      .arg(turns)
                                      .arg(fit)
                                      .arg(contextual)
                                      .toLatin1();
                QTest::newRow(name.constData()) << turns << fit << contextual;
            }
        }
    }
}

void main_window_tests::canvas_tools_edits() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop tools only");
#else
    QFETCH(int, turns);
    QFETCH(bool, fit);
    QFETCH(bool, contextual);
    canvas_tests::fixture f;
    f.editor->set_context_inspector(contextual);
    auto* cell = f.board.active_cell();
    cell->set_quarter_turns(turns);
    cell->set_keep_frame_aspect_ratio(fit);
    auto* tool = f.control<QComboBox>("canvas_tool");
    auto* target = f.control<QComboBox>("canvas_target");
    canvas_tests::activate(tool, 2);
    canvas_tests::activate(target, target->findData("north"));
    QCoreApplication::processEvents();
    QRectF frame(0, 0, cell->width(), cell->height());
    if (fit) {
        const double aspect = cell->effective_display_aspect();
        if (frame.width() / frame.height() > aspect) {
            const double width = frame.height() * aspect;
            frame.setLeft((cell->width() - width) / 2);
            frame.setWidth(width);
        } else {
            const double height = frame.width() / aspect;
            frame.setTop((cell->height() - height) / 2);
            frame.setHeight(height);
        }
    }
    const QPointF display[] { { 80, 70 }, { 30, 80 }, { 20, 30 }, { 70, 20 } };
    const QPoint vertex(
        qRound(frame.left() + frame.width() * display[turns].x() / 100),
        qRound(frame.top() + frame.height() * display[turns].y() / 100)
    );
    QSignalSpy selected(cell, &stream_cell::line_edit_point_selected);
    QTest::mouseClick(cell, Qt::LeftButton, Qt::NoModifier, vertex);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.last().at(0).toInt(), 1);
    const auto before = f.session.active_line_edit()->points_pct;
    QTest::keyClick(cell, Qt::Key_Right);
    const auto after = f.session.active_line_edit()->points_pct;
    QCOMPARE(after[0], before[0]);
    QVERIFY(after[1] != before[1]);
    if (turns % 2 == 0) {
        QCOMPARE(after[1].y(), before[1].y());
        QVERIFY((after[1].x() > before[1].x()) == (turns == 0));
    } else {
        QCOMPARE(after[1].x(), before[1].x());
        QVERIFY((after[1].y() > before[1].y()) == (turns == 3));
    }
    f.control<QPushButton>("canvas_undo")->click();
    QCOMPARE(f.session.active_line_edit()->points_pct, before);
    f.control<QPushButton>("canvas_redo")->click();
    QCOMPARE(f.session.active_line_edit()->points_pct, after);
    f.control<QPushButton>("canvas_revert")->click();
    QCOMPARE(f.session.active_line_edit()->points_pct, before);
    QVERIFY(f.editor->move_line_edit_point(1, { 75, 60 }));
    bool asked = false;
    QTimer::singleShot(0, &f.board, [&] {
        auto* dialog
            = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            asked = true;
            dialog->button(QMessageBox::Cancel)->click();
        }
    });
    canvas_tests::activate(target, target->findData("south"));
    QVERIFY(asked);
    QCOMPARE(target->currentData().toString(), QStringLiteral("north"));
    QCOMPARE(f.session.active_line_edit()->points_pct[1], QPointF(75, 60));
    QSignalSpy saved(
        &f.settings, &settings_panel::active_line_edit_save_requested
    );
    auto* name = f.control<QLineEdit>("canvas_name");
    name->setFocus();
    name->selectAll();
    QTest::keyClicks(name, "variant");
    f.control<QPushButton>("canvas_save")->click();
    QCOMPARE(saved.count(), 1);
    const auto request = qvariant_cast<line_edit_request>(saved.last().at(0));
    QCOMPARE(request.stream_name, QStringLiteral("one"));
    QCOMPARE(request.source_line_name, QStringLiteral("north"));
    QCOMPARE(request.profile.name, QStringLiteral("variant"));
    QCOMPARE(request.profile.width_text, QStringLiteral("string_heavy"));
    QVERIFY(request.profile.closed);
    QCOMPARE(request.points_pct[1], QPointF(75, 60));
    QVERIFY(!f.session.active_line_edit().has_value());
    QVERIFY(!f.control<QPushButton>("canvas_save")->isEnabled());
    QVERIFY(!cell->has_line_edit_preview());
#endif
}

void main_window_tests::canvas_tools_context() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop tools only");
#else
    canvas_tests::fixture f;
    auto* tool = f.control<QComboBox>("canvas_tool");
    auto* target = f.control<QComboBox>("canvas_target");
    canvas_tests::activate(tool, 2);
    canvas_tests::activate(target, target->findData("north"));
    QVERIFY(f.editor->move_line_edit_point(1, { 65, 50 }));
    auto* first = f.board.active_cell();
    static_cast<void>(f.streams.set_active_stream("two"));
    QVERIFY(f.board.active_cell() != first);
    QVERIFY(!f.session.active_line_edit().has_value());
    QCOMPARE(target->currentIndex(), 0);
    canvas_tests::activate(tool, 2);
    canvas_tests::activate(target, target->findData("north"));
    QCOMPARE(f.session.active_line_edit()->stream_name, QStringLiteral("two"));
    QCOMPARE(f.session.active_line_edit()->points_pct[1], QPointF(70, 80));
    const auto request = f.session.active_line_edit();
    static_cast<void>(f.streams.set_active_stream("two"));
    QCOMPARE(f.session.active_line_edit()->points_pct, request->points_pct);
    QCOMPARE(target->currentData().toString(), QStringLiteral("north"));
    f.session.replace_stream_lines("two", {});
    f.bridge.sync_active_persistent("two", f.session);
    QCOMPARE(target->count(), 1);
    QVERIFY(!f.session.active_line_edit().has_value());
    QCOMPARE(tool->currentIndex(), 1); // Qt leaves the now-disabled Edit tab.
    QVERIFY(!f.board.active_cell()->has_line_edit_preview());
    auto* modes = static_cast<QStandardItemModel*>(tool->model());
    QVERIFY(!modes->item(2)->isEnabled());
    static_cast<void>(f.streams.set_active_stream({}));
    QVERIFY(!tool->isEnabled());
    QVERIFY(
        !f.tools->isVisible()
    ); // Active container is hidden, not user intent.
    QCOMPARE(f.board.grid_mode()->layout_cell_count(), 2);
#endif
}
