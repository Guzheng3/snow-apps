#include "screenshotpinnedresizegeometry.h"

#include <QCoreApplication>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace resize_geometry = screenshot_pinned_resize_geometry;
using DragHandle = resize_geometry::DragHandle;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

QPoint fixedAnchor(const QRect& rect, DragHandle handle) {
    switch (handle) {
    case DragHandle::TopLeft:
        return rect.bottomRight();
    case DragHandle::Top:
    case DragHandle::TopRight:
        return rect.bottomLeft();
    case DragHandle::Right:
    case DragHandle::BottomRight:
    case DragHandle::Bottom:
        return rect.topLeft();
    case DragHandle::BottomLeft:
    case DragHandle::Left:
        return rect.topRight();
    }
    return {};
}

void testEveryHandlePreservesItsFixedAnchor() {
    const QSize baseline(320, 180);
    const QRect reference(QPoint(100, 200), baseline);
    struct TestCase {
        DragHandle handle;
        QRect proposed;
    };
    const std::vector<TestCase> cases{
        {DragHandle::TopLeft, QRect(20, 110, 400, 270)},
        {DragHandle::Top, QRect(100, 110, 320, 270)},
        {DragHandle::TopRight, QRect(100, 110, 480, 270)},
        {DragHandle::Right, QRect(100, 200, 400, 180)},
        {DragHandle::BottomRight, QRect(100, 200, 480, 270)},
        {DragHandle::Bottom, QRect(100, 200, 320, 270)},
        {DragHandle::BottomLeft, QRect(20, 200, 400, 270)},
        {DragHandle::Left, QRect(20, 200, 400, 180)},
    };

    for (const TestCase& testCase : cases) {
        QRect result;
        require(resize_geometry::proportionalResizeRect(testCase.proposed, reference, baseline,
                                                        testCase.handle, 0.1, 5.0, &result),
                "a valid resize proposal should produce a result");
        require(fixedAnchor(result, testCase.handle) == fixedAnchor(reference, testCase.handle),
                "resizing should preserve the fixed opposite anchor");
        require(
            qAbs(result.height() - qRound(result.width() * static_cast<double>(baseline.height()) /
                                          baseline.width())) <= 1,
            "resizing should preserve the baseline aspect ratio");
    }
}

void testDraggedEdgeDeterminesScale() {
    const QSize baseline(320, 180);
    const QRect reference(QPoint(100, 200), baseline);
    QRect horizontal;
    QRect vertical;
    QRect corner;

    require(resize_geometry::proportionalResizeRect(QRect(100, 200, 400, 181), reference, baseline,
                                                    DragHandle::Right, 0.1, 5.0, &horizontal) &&
                horizontal.size() == QSize(400, 225),
            "a horizontal handle should derive scale from the proposed width");
    require(resize_geometry::proportionalResizeRect(QRect(100, 200, 321, 270), reference, baseline,
                                                    DragHandle::Bottom, 0.1, 5.0, &vertical) &&
                vertical.size() == QSize(480, 270),
            "a vertical handle should derive scale from the proposed height");
    require(resize_geometry::proportionalResizeRect(QRect(100, 200, 400, 270), reference, baseline,
                                                    DragHandle::BottomRight, 0.1, 5.0, &corner) &&
                corner.size() == QSize(480, 270),
            "a corner handle should contain both proposed dimensions");
}

void testScaleLimitsUseExactBaselineMultiples() {
    const QSize baseline(853, 479);
    const QRect reference(QPoint(10, 20), baseline);
    QRect minimum;
    QRect maximum;

    require(resize_geometry::proportionalResizeRect(QRect(10, 20, 1, 1), reference, baseline,
                                                    DragHandle::Right, 0.1, 5.0, &minimum) &&
                minimum.size() == resize_geometry::scaledSize(baseline, 0.1),
            "resizing below the minimum should clamp to the minimum scale");
    require(resize_geometry::proportionalResizeRect(QRect(10, 20, 9999, 9999), reference, baseline,
                                                    DragHandle::BottomRight, 0.1, 5.0, &maximum) &&
                maximum.size() == resize_geometry::scaledSize(baseline, 5.0),
            "resizing above the maximum should clamp to the maximum scale");
}

void testInvalidInputsAreRejected() {
    QRect result;
    require(!resize_geometry::proportionalResizeRect({}, QRect(0, 0, 100, 100), QSize(100, 100),
                                                     DragHandle::Right, 0.1, 5.0, &result),
            "an empty proposal should be rejected");
    require(!resize_geometry::proportionalResizeRect(QRect(0, 0, 100, 100), QRect(0, 0, 100, 100),
                                                     QSize(100, 100), DragHandle::Right, 2.0, 1.0,
                                                     &result),
            "an inverted scale range should be rejected");
    require(resize_geometry::scaledSize({}, 1.0).isEmpty(),
            "an invalid baseline should not produce a scaled size");
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);

    try {
        testEveryHandlePreservesItsFixedAnchor();
        testDraggedEdgeDeterminesScale();
        testScaleLimitsUseExactBaselineMultiples();
        testInvalidInputsAreRejected();
    } catch (const std::exception& error) {
        std::cerr << "screenshot pinned resize geometry test failure: " << error.what() << '\n';
        return 1;
    }

    std::cout << "screenshot pinned resize geometry tests passed\n";
    return 0;
}
