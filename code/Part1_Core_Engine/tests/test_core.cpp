// Part 1 tests: escape-time maths, pixel mapping, threading, subdivision, infinite-zoom path.
#include <cmath>
#include <cstdio>

#include "explore.hpp"
#include "fractal.hpp"

using namespace fractal;

static int failures = 0;
#define CHECK(cond)                                             \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
            ++failures;                                         \
        }                                                       \
    } while (0)

int main() {
    // Mandelbrot: z0 = 0, c = point.
    CHECK(escapeTime({0, 0}, {0, 0}, 100) == 100);    // origin is inside
    CHECK(escapeTime({0, 0}, {-1, 0}, 100) == 100);   // period-2 cycle 0,-1,0,-1
    CHECK(escapeTime({0, 0}, {-2, 0}, 100) == 100);   // boundary point stays at 2
    CHECK(escapeTime({0, 0}, {2, 0}, 100) == 2);      // 2 -> 6
    CHECK(escapeTime({0, 0}, {1, 0}, 100) == 3);      // 1, 2, 5
    CHECK(escapeTime({0, 0}, {3, 3}, 100) == 1);      // escapes at once
    CHECK(escapeTime({0, 0}, {0.25, 0}, 500) == 500); // cusp stays bounded
    CHECK(escapeTime({0, 0}, {1, 0}, 2) == 2);        // iteration limit respected
    // Julia: c fixed, z0 = point.
    CHECK(escapeTime({0.5, 0}, {0, 0}, 100) == 100);  // |z| < 1 converges to 0
    CHECK(escapeTime({2, 0}, {0, 0}, 100) == 1);      // 4 > 2
    CHECK(escapeTime({1, 0}, {0, 0}, 100) == 100);    // fixed point

    // Pixel mapping: centre pixel maps to the view centre; top row is above bottom row.
    View v;
    Complex mid = pixelToPlane(v, 50, 50, 101, 101, 1.0);
    CHECK(std::abs(mid.real() - v.cx) < 1e-12 && std::abs(mid.imag() - v.cy) < 1e-12);
    CHECK(pixelToPlane(v, 5, 0, 10, 10, 1.0).imag() > pixelToPlane(v, 5, 9, 10, 10, 1.0).imag());

    // Parallel result must be identical to serial for every thread count.
    for (Kind k : {Kind::Mandelbrot, Kind::Julia}) {
        Settings s;
        s.kind = k;
        s.maxIter = 200;
        View view = defaultView(k);
        Grid ref(160, 90);
        renderSerial(ref, view, s, 1.0);
        for (unsigned t : {1u, 2u, 3u, 7u, 0u}) {
            Grid g(160, 90);
            renderParallel(g, view, s, 1.0, t);
            CHECK(g.data() == ref.data());
        }
    }

    // Subdivision: near-identical to baseline, with far fewer evaluations.
    {
        Settings s;
        s.maxIter = 300;
        View view = defaultView(Kind::Mandelbrot);
        Grid ref(300, 200), sub(300, 200);
        renderSerial(ref, view, s, 1.0);
        std::size_t evals = renderSubdivision(sub, view, s, 1.0);
        std::size_t diff = 0;
        bool allSet = true;
        for (std::size_t i = 0; i < ref.data().size(); ++i) {
            diff += ref.data()[i] != sub.data()[i];
            allSet &= sub.data()[i] >= 0;
        }
        std::printf("subdivision: %zu evals of %zu pixels, %zu differing\n", evals, ref.data().size(), diff);
        CHECK(evals < ref.data().size());
        CHECK(diff * 100 < ref.data().size()); // < 1% differences
        CHECK(allSet);
    }

    // Tiny grids must not crash.
    {
        Settings s;
        Grid g(1, 1);
        renderParallel(g, View{}, s, 1.0, 4);
        Grid h(2, 1);
        renderSubdivision(h, View{}, s, 1.0);
    }

    // Infinite-zoom mapping: reversible, target stays on screen at every depth,
    // and the deepest view is still resolvable by doubles.
    {
        double top = explore::maxDepth();
        CHECK(top > 35 && top < 50);
        View v0;
        Settings s0;
        explore::viewAtDepth(0, v0, s0);
        CHECK(v0.cx == defaultView(Kind::Mandelbrot).cx && v0.halfWidth == defaultView(Kind::Mandelbrot).halfWidth);
        double prevHalf = 1e9;
        bool onScreen = true, shrinking = true;
        for (double d = 0; d <= top; d += 0.25) {
            View vv;
            Settings st;
            explore::viewAtDepth(d, vv, st);
            onScreen &= std::abs(vv.cx - (-0.7436438870371587)) < vv.halfWidth &&
                        std::abs(vv.cy - 0.13182590420531197) < vv.halfWidth;
            shrinking &= vv.halfWidth < prevHalf;
            prevHalf = vv.halfWidth;
        }
        CHECK(onScreen);
        CHECK(shrinking);
        View a, b;
        Settings sa, sb;
        explore::viewAtDepth(20.5, a, sa);
        explore::viewAtDepth(20.5, b, sb);
        CHECK(a.cx == b.cx && a.halfWidth == b.halfWidth && sa.maxIter == sb.maxIter); // pure function
    }

    if (failures) std::printf("Part 1: %d test(s) FAILED\n", failures);
    else std::printf("Part 1: all tests passed\n");
    return failures ? 1 : 0;
}
