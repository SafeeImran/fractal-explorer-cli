#include "fractal.hpp"

#include <algorithm>
#include <atomic>
#include <thread>

namespace fractal {

View defaultView(Kind kind) {
    View v;
    if (kind == Kind::Julia) {
        v.cx = 0.0;
        v.cy = 0.0;
        v.halfWidth = 1.6;
    }
    return v;
}

const char* kindName(Kind kind) { return kind == Kind::Mandelbrot ? "Mandelbrot" : "Julia"; }

int escapeTime(Complex z0, Complex c, int maxIter) {
    Complex z = z0;
    for (int n = 1; n <= maxIter; ++n) {
        z = z * z + c;
        if (std::norm(z) > 4.0) return n; // |z| > 2
    }
    return maxIter;
}

int pixelValue(const Settings& s, Complex p) {
    return s.kind == Kind::Mandelbrot ? escapeTime(Complex(0, 0), p, s.maxIter)
                                      : escapeTime(p, s.juliaC, s.maxIter);
}

Complex pixelToPlane(const View& v, int x, int y, int w, int h, double cellAspect) {
    double halfHeight = v.halfWidth * (static_cast<double>(h) / w) * cellAspect;
    double re = v.cx + ((x + 0.5) / w * 2.0 - 1.0) * v.halfWidth;
    double im = v.cy - ((y + 0.5) / h * 2.0 - 1.0) * halfHeight; // row 0 is the top
    return {re, im};
}

static void renderRow(Grid& g, int y, const View& v, const Settings& s, double aspect) {
    for (int x = 0; x < g.width(); ++x)
        g.at(x, y) = pixelValue(s, pixelToPlane(v, x, y, g.width(), g.height(), aspect));
}

unsigned defaultThreads() {
    unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

void renderSerial(Grid& g, const View& v, const Settings& s, double aspect) {
    for (int y = 0; y < g.height(); ++y) renderRow(g, y, v, s, aspect);
}

void renderParallel(Grid& g, const View& v, const Settings& s, double aspect, unsigned threads) {
    if (threads == 0) threads = defaultThreads();
    threads = std::min<unsigned>(threads, static_cast<unsigned>(g.height()));
    if (threads <= 1) {
        renderSerial(g, v, s, aspect);
        return;
    }
    // Rows near the set cost far more than rows far away, so a shared atomic row
    // counter balances the load better than fixed contiguous blocks.
    std::atomic<int> nextRow{0};
    auto worker = [&]() {
        for (;;) {
            int y = nextRow.fetch_add(1, std::memory_order_relaxed);
            if (y >= g.height()) return;
            renderRow(g, y, v, s, aspect); // each row is written by exactly one thread
        }
    };
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
}

namespace {

struct Subdivider {
    Grid& g;
    const View& v;
    const Settings& s;
    double aspect;
    std::size_t evals = 0;

    int eval(int x, int y) {
        int& cell = g.at(x, y);
        if (cell < 0) {
            cell = pixelValue(s, pixelToPlane(v, x, y, g.width(), g.height(), aspect));
            ++evals;
        }
        return cell;
    }

    // Rectangle with inclusive corners (x0,y0)-(x1,y1).
    void run(int x0, int y0, int x1, int y1) {
        bool uniform = true;
        int ref = eval(x0, y0);
        for (int x = x0; x <= x1; ++x) {
            uniform &= eval(x, y0) == ref;
            uniform &= eval(x, y1) == ref;
        }
        for (int y = y0; y <= y1; ++y) {
            uniform &= eval(x0, y) == ref;
            uniform &= eval(x1, y) == ref;
        }
        bool hasInterior = (x1 - x0 >= 2) && (y1 - y0 >= 2);
        if (!hasInterior) return;
        if (uniform) {
            for (int y = y0 + 1; y < y1; ++y)
                for (int x = x0 + 1; x < x1; ++x)
                    if (g.at(x, y) < 0) g.at(x, y) = ref;
            return;
        }
        if (x1 - x0 >= y1 - y0) {
            int mid = (x0 + x1) / 2;
            run(x0, y0, mid, y1);
            run(mid, y0, x1, y1);
        } else {
            int mid = (y0 + y1) / 2;
            run(x0, y0, x1, mid);
            run(x0, mid, x1, y1);
        }
    }
};

} // namespace

std::size_t renderSubdivision(Grid& g, const View& v, const Settings& s, double aspect) {
    g.fill(-1);
    Subdivider sub{g, v, s, aspect};
    sub.run(0, 0, g.width() - 1, g.height() - 1);
    return sub.evals;
}

} // namespace fractal
