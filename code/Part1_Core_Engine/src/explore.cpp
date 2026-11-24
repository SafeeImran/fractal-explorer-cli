#include "explore.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#include "output.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace fractal;

namespace explore {
namespace {

// A point on the boundary of the set where the structure keeps repeating (a
// Misiurewicz point in "Seahorse Valley"), so there is always more to see.
constexpr double kTargetRe = -0.7436438870371587;
constexpr double kTargetIm = 0.13182590420531197;
constexpr double kMinHalfWidth = 2e-13; // below this, doubles cannot separate neighbouring pixels

std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

struct Input {
    int dir = 0;        // +1 forward (zoom in), -1 backward (zoom out)
    int speedSteps = 0; // +1 / -1 per Up / Down press
    bool toggleAuto = false;
    bool quit = false;
};

using Clock = std::chrono::steady_clock;

#ifdef _WIN32

class Keyboard {
public:
    Input poll() {
        Input in;
        // Real held-key state: no keyboard auto-repeat delay, so motion starts at once.
        bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
        bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
        in.dir = (left ? 1 : 0) - (right ? 1 : 0);
        while (_kbhit()) { // drain the console buffer so keystrokes never pile up
            int ch = _getch();
            if (ch == 0 || ch == 0xE0) {
                int k = _getch();
                if (k == 72) ++in.speedSteps; // Up
                if (k == 80) --in.speedSteps; // Down
            } else if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
                in.quit = true;
            } else if (ch == 'p' || ch == 'P' || ch == ' ') {
                in.toggleAuto = true;
            }
        }
        return in;
    }
};

#else

// Terminals only send key events (with an initial repeat delay), not key-up, so a
// key counts as "held" for a short window after its last event.
class Keyboard {
public:
    Keyboard() {
        tcgetattr(STDIN_FILENO, &saved_);
        termios raw = saved_;
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    ~Keyboard() { tcsetattr(STDIN_FILENO, TCSANOW, &saved_); }
    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;

    Input poll() {
        Input in;
        auto now = Clock::now();
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        for (ssize_t i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == 27) {
                if (i + 2 < n &&buf[i + 1] == '[') {
                    char k = buf[i + 2];
                    i += 2;
                    if (k == 'D') event(now, leftUntil_, leftLast_);
                    else if (k == 'C') event(now, rightUntil_, rightLast_);
                    else if (k == 'A') ++in.speedSteps;
                    else if (k == 'B') --in.speedSteps;
                } else if (i + 1 >= n) {
                    in.quit = true; // a bare Esc
                }
            } else if (ch == 'q' || ch == 'Q' || ch == 3) {
                in.quit = true;
            } else if (ch == 'p' || ch == 'P' || ch == ' ') {
                in.toggleAuto = true;
            }
        }
        bool left = now < leftUntil_, right = now < rightUntil_;
        in.dir = (left ? 1 : 0) - (right ? 1 : 0);
        return in;
    }

private:
    using TP = Clock::time_point;
    static void event(TP now, TP& until, TP& last) {
        bool repeating = now - last < std::chrono::milliseconds(200);
        until = now + std::chrono::milliseconds(repeating ? 120 : 550);
        last = now;
    }
    termios saved_{};
    TP leftUntil_{}, rightUntil_{}, leftLast_{}, rightLast_{};
};

#endif

std::string humanZoom(double depth) {
    char buf[32];
    double zoom = std::exp2(depth);
    if (zoom < 1000) std::snprintf(buf, sizeof buf, "x%.1f", zoom);
    else std::snprintf(buf, sizeof buf, "x%.2e", zoom);
    return buf;
}

} // namespace

double maxDepth() { return std::log2(defaultView(Kind::Mandelbrot).halfWidth / kMinHalfWidth); }

void viewAtDepth(double depth, View& v, Settings& s) {
    View base = defaultView(Kind::Mandelbrot);
    double zoom = std::exp2(depth);
    v.halfWidth = base.halfWidth / zoom;
    // The offset from the target shrinks slightly faster than the view width, so the
    // target drifts toward the screen centre as we dive.
    double pull = std::exp2(-1.15 * depth);
    v.cx = kTargetRe + (base.cx - kTargetRe) * pull;
    v.cy = kTargetIm + (base.cy - kTargetIm) * pull;
    s.kind = Kind::Mandelbrot;
    s.maxIter = std::min(4000, static_cast<int>(100 + 75 * depth));
}

void run(const Options& opt) {
    g_stop = false;
    std::signal(SIGINT, onSignal);
    Keyboard keys;

    const double limit = maxDepth();
    const auto period = std::chrono::microseconds(1000000 / std::max(1, opt.fps));

    double depth = 0, velocity = 0; // velocity in octaves (2x zooms) per second
    double speed = 1.5;
    bool autoplay = opt.autoplay;
    bool dirty = true;
    int lastCols = -1, lastRows = -1, frames = 0;

    std::cout << "\x1b[2J\x1b[?25l" << std::flush;
    auto last = Clock::now();
    while (!g_stop) {
        auto start = Clock::now();
        double dt = std::min(0.1, std::chrono::duration<double>(start - last).count());
        last = start;

        Input in = keys.poll();
        if (in.quit) break;
        if (in.toggleAuto) autoplay = !autoplay;
        if (in.speedSteps) {
            speed = std::clamp(speed * std::pow(1.3, in.speedSteps), 0.3, 6.0);
            dirty = true;
        }
        int dir = in.dir;
        if (dir != 0) autoplay = false; // manual control wins
        else if (autoplay) dir = 1;

        // Ease toward the target speed so starts and stops feel smooth.
        velocity += (dir * speed - velocity) * std::min(1.0, dt * 8.0);
        if (dir == 0 && std::abs(velocity) < 0.01) velocity = 0;
        double next = std::clamp(depth + velocity * dt, 0.0, limit);
        if (next != depth) dirty = true;
        if ((next <= 0 && velocity < 0) || (next >= limit && velocity > 0)) {
            velocity = 0;
            if (next >= limit) autoplay = false;
        }
        depth = next;

        int termCols, termRows;
        output::terminalSize(termCols, termRows);
        if (termCols != lastCols || termRows != lastRows) {
            std::cout << "\x1b[2J" << std::flush;
            lastCols = termCols;
            lastRows = termRows;
            dirty = true;
        }

        if (dirty) {
            dirty = false;
            int cols = std::clamp(termCols - 1, 20, 220);
            int rows = std::clamp(termRows - 4, 8, 80);
            View v;
            Settings s;
            viewAtDepth(depth, v, s);
            Grid g(cols, rows);
            renderParallel(g, v, s, 2.0, opt.threads);

            std::string frame = "\x1b[H" + output::toAsciiBands(g, s.maxIter, 32, opt.colour);
            int barLen = std::clamp(cols - 50, 10, 40);
            int filled = static_cast<int>(barLen * depth / limit + 0.5);
            frame += "\x1b[0m\x1b[KInfinite zoom - Seahorse Valley   [" + std::string(filled, '#') +
                     std::string(barLen - filled, '-') + "]\n\x1b[K";
            frame += "zoom " + humanZoom(depth) + "   iterations " + std::to_string(s.maxIter) + "   speed " +
                     std::to_string(speed).substr(0, 3) + (autoplay ? "   AUTO" : "");
            if (depth >= limit - 1e-9) frame += "   << limit of double precision - press Right to back out >>";
            frame += "\n\x1b[K<- forward   -> backward   Up/Down speed   P auto   Q quit";
            std::cout << frame << std::flush;
        }

        if (opt.maxFrames > 0 && ++frames >= opt.maxFrames) break;
        auto spent = Clock::now() - start;
        if (spent < period) std::this_thread::sleep_for(period - spent);
    }
    std::cout << "\x1b[0m\x1b[?25h\n" << std::flush;
    std::signal(SIGINT, SIG_DFL);
}

} // namespace explore
