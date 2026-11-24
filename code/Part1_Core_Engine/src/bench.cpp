#include "bench.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <vector>

namespace bench {

using Clock = std::chrono::steady_clock;

template <class F>
static double bestMs(int reps, F&& f) {
    double best = 1e18;
    for (int i = 0; i < reps; ++i) {
        auto t0 = Clock::now();
        f();
        double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        best = std::min(best, ms);
    }
    return best;
}

void run(const fractal::View& v, const fractal::Settings& s, int w, int h, int reps,
         const std::string& csvPath) {
    using namespace fractal;
    const double aspect = 1.0;
    unsigned hw = defaultThreads();
    std::printf("Benchmark: %s, %dx%d px, maxIter=%d, best of %d runs, %u hardware threads\n",
                kindName(s.kind), w, h, s.maxIter, reps, hw);

    std::set<unsigned> counts{1, 2, 4, hw};
    for (unsigned t = 8; t <= hw; t *= 2) counts.insert(t);

    Grid reference(w, h);
    std::vector<std::vector<std::string>> rows;
    std::printf("\n%-22s %12s %10s %12s\n", "mode", "time (ms)", "speed-up", "efficiency");
    double base = 0;
    for (unsigned t : counts) {
        if (t > hw * 2) continue;
        Grid g(w, h);
        double ms = bestMs(reps, [&] { renderParallel(g, v, s, aspect, t); });
        if (t == 1) { base = ms; reference = g; }
        double speed = base / ms;
        char name[32];
        std::snprintf(name, sizeof name, "%u thread%s", t, t == 1 ? "" : "s");
        std::printf("%-22s %12.1f %9.2fx %11.0f%%\n", name, ms, speed, 100.0 * speed / t);
        char buf[4][32];
        std::snprintf(buf[0], 32, "%u", t); std::snprintf(buf[1], 32, "%.3f", ms);
        std::snprintf(buf[2], 32, "%.3f", speed); std::snprintf(buf[3], 32, "%.3f", speed / t);
        rows.push_back({"threads", buf[0], buf[1], buf[2], buf[3]});
        if (t != 1 && g.data() != reference.data()) std::printf("  WARNING: output differs from serial!\n");
    }

    // Optional extension: subdivision vs the simple baseline.
    Grid sub(w, h);
    std::size_t evals = 0;
    double ms = bestMs(reps, [&] { evals = renderSubdivision(sub, v, s, aspect); });
    std::size_t diff = 0;
    for (std::size_t i = 0; i < sub.data().size(); ++i) diff += sub.data()[i] != reference.data()[i];
    std::printf("%-22s %12.1f %9.2fx %12s\n", "subdivision (1 thr)", ms, base / ms, "-");
    std::printf("  evaluated %zu of %zu pixels (%.1f%%); %zu pixels differ from baseline (%.3f%%)\n",
                evals, sub.data().size(), 100.0 * evals / sub.data().size(), diff,
                100.0 * diff / sub.data().size());

    if (!csvPath.empty()) {
        std::ofstream f(csvPath);
        if (!f) { std::printf("Could not write %s\n", csvPath.c_str()); return; }
        f << "mode,threads,time_ms,speedup,efficiency\n";
        for (auto& r : rows) f << r[0] << ',' << r[1] << ',' << r[2] << ',' << r[3] << ',' << r[4] << '\n';
        f << "subdivision,1," << ms << ',' << base / ms << ",\n";
        std::printf("\nCSV written to %s\n", csvPath.c_str());
    }
}

} // namespace bench
