#include "demo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "fractal.hpp"
#include "output.hpp"

using namespace fractal;

namespace demo {
namespace {

std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

// Terminal size minus room for the caption lines.
void terminalSize(int& cols, int& rows) {
    output::terminalSize(cols, rows);
    cols = std::clamp(cols - 1, 20, 220);
    rows = std::clamp(rows - 4, 8, 80); // leave room for the caption lines
}

double smooth(double t) { return t * t * (3 - 2 * t); }

struct Slide {
    std::string title;
    std::string note;
    // Fills view/settings for progress t in [0,1].
    void (*frame)(double t, View& v, Settings& s);
};

// Zoom from the default view into (cx, cy), reaching `depth` times magnification
// at t = 0.85, then hold. Iteration limit grows with depth so detail stays crisp.
void zoomInto(double t, View& v, Settings& s, double cx, double cy, double depth) {
    double k = smooth(std::min(1.0, t / 0.85));
    View base = defaultView(Kind::Mandelbrot);
    double zoom = std::pow(depth, k);
    v.halfWidth = base.halfWidth / zoom;
    // Glide the centre so the target stays on screen while zooming.
    double glide = 1.0 - std::pow(1.0 - k, 2.0);
    v.cx = base.cx + (cx - base.cx) * glide;
    v.cy = base.cy + (cy - base.cy) * glide;
    s.kind = Kind::Mandelbrot;
    s.maxIter = std::min(2500, static_cast<int>(80 + 70 * std::log2(zoom)));
}

void seahorse(double t, View& v, Settings& s) { zoomInto(t, v, s, -0.743643887, 0.131825904, 6000); }
void spiral(double t, View& v, Settings& s) { zoomInto(t, v, s, -0.77568377, 0.13646737, 20000); }
void satellite(double t, View& v, Settings& s) { zoomInto(t, v, s, -1.768778833, -0.001738996, 3000); }

void juliaOrbit(double t, View& v, Settings& s) {
    double a = 6.283185307179586 * t;
    s.kind = Kind::Julia;
    s.juliaC = std::polar(0.7885, a);
    s.maxIter = 120;
    v = defaultView(Kind::Julia);
}

void juliaDust(double t, View& v, Settings& s) {
    // c walks from the connected "dragon" regime out into disconnected dust.
    double k = 0.5 - 0.5 * std::cos(6.283185307179586 * t);
    s.kind = Kind::Julia;
    s.juliaC = Complex(-0.8 + 0.8 * k, 0.156 + 0.3 * k);
    s.maxIter = 150;
    v = defaultView(Kind::Julia);
}

const std::vector<Slide>& slides() {
    static const std::vector<Slide> list = {
        {"Mandelbrot set - Seahorse Valley", "z(n+1) = z(n)^2 + c, z(0) = 0   |   colour = iterations until |z| > 2", seahorse},
        {"Julia sets - c orbits a circle", "z(n+1) = z(n)^2 + c, c fixed per picture, z(0) = pixel", juliaOrbit},
        {"Mandelbrot set - Double Spiral", "Zooming ~20000x: the same structure repeats at every scale", spiral},
        {"Julia sets - connected to dust", "Julia set is connected exactly when c lies inside the Mandelbrot set", juliaDust},
        {"Mandelbrot set - a tiny copy of itself", "Self-similarity: a mini Mandelbrot hides on the real axis", satellite},
    };
    return list;
}

} // namespace

void run(const Options& opt) {
    std::signal(SIGINT, onSignal);
    g_stop = false;
    int cols, rows;
    terminalSize(cols, rows);

    using Clock = std::chrono::steady_clock;
    const auto framePeriod = std::chrono::microseconds(1000000 / std::max(1, opt.fps));
    const int framesPerSlide = std::max(1, static_cast<int>(opt.secondsPerSlide * opt.fps));

    std::cout << "\x1b[2J\x1b[?25l" << std::flush; // clear, hide cursor
    do {
        int idx = 0;
        for (const Slide& slide : slides()) {
            ++idx;
            for (int f = 0; f < framesPerSlide && !g_stop; ++f) {
                auto start = Clock::now();
                double t = framesPerSlide == 1 ? 1.0 : static_cast<double>(f) / (framesPerSlide - 1);
                View v;
                Settings s;
                slide.frame(t, v, s);

                Grid g(cols, rows);
                renderParallel(g, v, s, 2.0, opt.threads);
                std::string frame = "\x1b[H" + output::toAscii(g, s.maxIter, opt.colour);
                frame += "\x1b[0m\x1b[K" + slide.title + "  [" + std::to_string(idx) + "/" +
                         std::to_string(slides().size()) + "]\n\x1b[K" + slide.note + "\n\x1b[K";
                if (s.kind == Kind::Julia)
                    frame += "c = " + std::to_string(s.juliaC.real()).substr(0, 7) + (s.juliaC.imag() < 0 ? " - " : " + ") +
                             std::to_string(std::abs(s.juliaC.imag())).substr(0, 6) + "i";
                else
                    frame += "zoom x" + std::to_string(static_cast<long>(defaultView(Kind::Mandelbrot).halfWidth / v.halfWidth)) +
                             "   iterations " + std::to_string(s.maxIter);
                frame += "   (Ctrl+C to stop)";
                std::cout << frame << std::flush;

                auto spent = Clock::now() - start;
                if (spent < framePeriod) std::this_thread::sleep_for(framePeriod - spent);
            }
            if (g_stop) break;
        }
    } while (opt.loop && !g_stop);
    std::cout << "\x1b[0m\x1b[?25h\n" << std::flush; // restore cursor
    std::signal(SIGINT, SIG_DFL);
}

} // namespace demo
