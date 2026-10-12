#include "ui/main_window_tests.hpp"

#include "packing/layout/fixed_size_layout.hpp"
#include "packing/layout/free_order_layout.hpp"
#include "packing/layout/ordered_layout.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QScrollArea>
#include <QtTest/QtTest>

#include <algorithm>
#include <vector>

void main_window_tests::grid_order_basics() {
    stream_board board;
    auto* grid = board.grid_mode();
    for (const auto& name : { "charlie", "alpha", "bravo" }) {
        grid->add_stream(QString::fromLatin1(name));
    }
    const QStringList alphabetic { "alpha", "bravo", "charlie" };
    const QStringList chosen { "charlie", "bravo", "alpha", "hidden" };
    QCOMPARE(grid->stream_names(), alphabetic);
    QVERIFY(grid->stream_order().isEmpty());
    QVERIFY(!grid->set_stream_order({ "charlie", "charlie" }));
    QVERIFY(!grid->set_stream_order({ "alpha", "" }));
    QCOMPARE(grid->stream_names(), alphabetic);
    QVERIFY(grid->set_stream_order(chosen));
    QCOMPARE(
        grid->stream_names(), QStringList({ "charlie", "bravo", "alpha" })
    );
    QVERIFY(!grid->preserves_stream_order());
    auto* cell = grid->peek_stream_cell(QStringLiteral("bravo"));
    cell->initialize_manual_short_side(192.5);
    cell->set_quarter_turns(1);
    cell->set_draft_points_pct({ QPointF(10, 20), QPointF(80, 90) });
    const auto draft = cell->draft_points_pct();
    board.set_active_stream(QStringLiteral("bravo"));
    QCOMPARE(board.active_cell(), cell);
    QCOMPARE(grid->stream_names(), QStringList({ "charlie", "alpha" }));
    QCOMPARE(grid->stream_order(), chosen);
    board.set_active_stream(QStringLiteral("missing"));
    QCOMPARE(board.active_cell(), cell);
    QCOMPARE(cell->draft_points_pct(), draft);
    board.clear_active();
    QCOMPARE(grid->peek_stream_cell(QStringLiteral("bravo")), cell);
    QCOMPARE(
        grid->stream_names(), QStringList({ "charlie", "bravo", "alpha" })
    );
    QCOMPARE(cell->manual_short_side(), 192.5);
    QCOMPARE(cell->quarter_turns(), 1);
    QVERIFY(cell->draft_points_pct().empty()); // Existing deactivation policy.
    grid->remove_stream(QStringLiteral("alpha")); // Hide doesn't forget intent.
    grid->add_stream(QStringLiteral("alpha"));
    QCOMPARE(
        grid->stream_names(), QStringList({ "charlie", "bravo", "alpha" })
    );
    grid->add_stream(QStringLiteral("hidden"));
    QCOMPARE(grid->stream_names(), chosen);
    grid->add_stream(QStringLiteral("new"));
    QCOMPARE(grid->stream_names(), chosen + QStringList { "new" });
    QVERIFY(grid->set_stream_order(chosen)); // No-op keeps policy and identity.
    QCOMPARE(grid->peek_stream_cell(QStringLiteral("bravo")), cell);
    QVERIFY(grid->set_stream_order({}));
    QCOMPARE(
        grid->stream_names(),
        QStringList({ "alpha", "bravo", "charlie", "hidden", "new" })
    );
}

void main_window_tests::grid_order_plans_data() {
    QTest::addColumn<bool>("fixed");
    QTest::addColumn<bool>("preserve");
    QTest::addColumn<bool>("vertical");
    for (const bool fixed : { false, true }) {
        for (const bool preserve : { false, true }) {
            for (const bool vertical : { false, true }) {
                const auto name = QStringLiteral("%1-%2-%3")
                                      .arg(fixed)
                                      .arg(preserve)
                                      .arg(vertical)
                                      .toLatin1();
                QTest::newRow(name.constData())
                    << fixed << preserve << vertical;
            }
        }
    }
}

void main_window_tests::grid_order_plans() {
    QFETCH(bool, fixed);
    QFETCH(bool, preserve);
    QFETCH(bool, vertical);
    grid_view grid;
    grid.resize(700, 340);
    grid.set_scroll_direction(
        vertical ? grid_view::scroll_direction::vertical
                 : grid_view::scroll_direction::horizontal
    );
    grid.set_preserve_stream_order(preserve);
    if (fixed) {
        grid.set_sizing_mode(grid_view::sizing_mode::fixed);
    }
    grid.show();
    for (const auto& name : { "alpha", "bravo", "charlie" }) {
        grid.add_stream(QString::fromLatin1(name));
    }
    const QStringList order { "charlie", "alpha", "bravo" };
    const std::vector<QSize> sizes { { 100, 250 }, { 300, 100 }, { 200, 200 } };
    for (int index = 0; index < 3; ++index) {
        auto* cell = grid.peek_stream_cell(order[index]);
        QImage image(
            sizes[static_cast<std::size_t>(index)], QImage::Format_RGB32
        );
        image.fill(Qt::black);
        QVERIFY(
            QMetaObject::invokeMethod(
                cell, "on_frame_changed", Qt::DirectConnection,
                Q_ARG(QVideoFrame, QVideoFrame(image))
            )
        );
        cell->set_quarter_turns(index % 2);
        cell->initialize_manual_short_side(160.0 + index * 32);
    }
    QVERIFY(grid.set_stream_order(order));
    QCoreApplication::processEvents();
    auto* scroll = grid.findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    // Compare indexed rectangles, not the solver's shelf traversal order.
    const auto check_plan = [&] {
        packing::extent viewport {
            static_cast<double>(scroll->viewport()->width() - 12),
            static_cast<double>(scroll->viewport()->height() - 12),
        };
        const auto policy = vertical ? packing::scroll_policy::vertical
                                     : packing::scroll_policy::horizontal;
        std::vector<packing::rectangle> expected;
        if (fixed) {
            std::vector<packing::fixed_size_item> items;
            for (const auto& name : order) {
                auto* cell = grid.peek_stream_cell(name);
                const double aspect = cell->effective_display_aspect();
                const double short_side = cell->manual_short_side();
                const packing::extent size = aspect >= 1.0
                    ? packing::extent { short_side * aspect, short_side }
                    : packing::extent { short_side, short_side / aspect };
                items.push_back({ .size = size });
                if (vertical) {
                    viewport.width = std::max(viewport.width, size.width);
                } else {
                    viewport.height = std::max(viewport.height, size.height);
                }
            }
            expected
                = packing::layout_fixed_size_rectangles(
                      {
                          .viewport = viewport,
                          .items = items,
                          .policy = policy,
                          .spacing = 6,
                          .algorithm = preserve
                              ? packing::fixed_size_layout_algorithm::
                                    preserve_order_shelf
                              : packing::fixed_size_layout_algorithm::automatic,
                      }
                )
                      .rectangles;
        } else {
            std::vector<packing::ordered_slot> items;
            for (const auto& name : order) {
                const auto* cell = grid.peek_stream_cell(name);
                items.push_back(
                    { .aspect = cell->source_frame_aspect(),
                      .rotated = cell->quarter_turns() % 2 != 0 }
                );
            }
            if (preserve) {
                expected = packing::layout_ordered_slots(
                               {
                                   .viewport = viewport,
                                   .items = items,
                                   .policy = policy,
                                   .minimum_short_side = 160,
                                   .spacing = 6,
                               }
                )
                               .rectangles;
            } else {
                expected = packing::layout_free_order_slots(
                               {
                                   .viewport = viewport,
                                   .items = items,
                                   .policy = policy,
                                   .minimum_short_side = 160,
                                   .spacing = 6,
                               }
                )
                               .rectangles;
            }
        }
        if (expected.size() != static_cast<std::size_t>(order.size())) {
            return false;
        }
        for (qsizetype index = 0; index < order.size(); ++index) {
            const auto& rect = expected[static_cast<std::size_t>(index)];
            const QRect geometry(
                6 + qRound(rect.x), 6 + qRound(rect.y),
                std::max(1, qRound(rect.width)),
                std::max(1, qRound(rect.height))
            );
            if (grid.peek_stream_cell(order[index])->geometry() != geometry) {
                return false;
            }
        }
        return true;
    };
    QTRY_VERIFY(check_plan());
    QCOMPARE(grid.stream_order(), order);
    grid.resize(840, 420);
    QTRY_VERIFY(check_plan());
    QCOMPARE(grid.stream_order(), order);
    QCOMPARE(grid.preserves_stream_order(), preserve);
}
