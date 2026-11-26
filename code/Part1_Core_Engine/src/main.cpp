// Fractal Explorer CLI - interactive text menu and scriptable flags.
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "bench.hpp"   // Part 2
#include "demo.hpp"    // Part 3
#include "explore.hpp" // Part 1
#include "fractal.hpp" // Part 1
#include "input.hpp"   // Part 3
#include "output.hpp"  // Part 2

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void enableAnsi() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
}
#else
static void enableAnsi() {}
#endif

using namespace fractal;

namespace {

constexpr double kTermAspect = 2.0; // terminal cells are about twice as tall as wide

struct State {
    Settings s;
    View v = defaultView(Kind::Mandelbrot);
    int cols = 78, rows = 28;
    unsigned threads = 0; // 0 = all hardware threads
    bool colour = false;
};

using input::parseDouble;
using input::parseInt;
using input::split;

void draw(const State& st) {
    Grid g(st.cols, st.rows);
    renderParallel(g, st.v, st.s, kTermAspect, st.threads);
    std::cout << output::toAscii(g, st.s.maxIter, st.colour);
    std::cout << kindName(st.s.kind) << " | centre " << st.v.cx << (st.v.cy < 0 ? " - " : " + ")
              << std::abs(st.v.cy) << "i | half-width " << st.v.halfWidth << " | iter " << st.s.maxIter;
    if (st.s.kind == Kind::Julia)
        std::cout << " | c = " << st.s.juliaC.real() << (st.s.juliaC.imag() < 0 ? " - " : " + ")
                  << std::abs(st.s.juliaC.imag()) << "i";
    std::cout << " | threads " << (st.threads ? st.threads : defaultThreads()) << '\n';
}

void help() {
    std::cout <<
        "Commands:\n"
        "  m | j [re im]      switch to Mandelbrot / Julia (optional Julia constant c)\n"
        "  + [f] | - [f]      zoom in / out (default factor 2, range 1.01-1000)\n"
        "  w a s d [f]        pan up/left/down/right by fraction f of the width (default 0.25)\n"
        "  center re im       set the view centre\n"
        "  i N                iteration limit (1-100000)\n"
        "  t N                worker threads (0 = all hardware threads)\n"
        "  size W H           ASCII size (10-300 x 5-120)\n"
        "  color              toggle ANSI colour\n"
        "  e [file W H]       export PPM (default fractal.ppm 800 600)\n"
        "  b                  benchmark 1..N threads and subdivision\n"
        "  demo               animated slideshow (Ctrl+C stops)\n"
        "  explore            infinite zoom driven by the arrow keys\n"
        "  r                  reset view      h  help      q  quit\n";
}

bool exportPPM(const State& st, const std::string& file, int w, int h) {
    Grid g(w, h);
    renderParallel(g, st.v, st.s, 1.0, st.threads);
    std::string err;
    if (!output::writePPM(file, g, st.s.maxIter, err)) {
        std::cout << "Export failed: " << err << '\n';
        return false;
    }
    std::cout << "Saved " << w << "x" << h << " image to " << file << '\n';
    return true;
}

void bad(const std::string& why) { std::cout << "Invalid input: " << why << " (type h for help)\n"; }

// Returns false when the user asked to quit.
bool handle(State& st, const std::string& line) {
    auto tok = split(line);
    if (tok.empty()) return true;
    const std::string& c = tok[0];

    if (c == "q" || c == "quit" || c == "exit") return false;
    if (c == "h" || c == "help" || c == "?") { help(); return true; }

    if (c == "m") {
        st.s.kind = Kind::Mandelbrot;
        st.v = defaultView(st.s.kind);
    } else if (c == "j") {
        if (tok.size() == 3) {
            auto re = parseDouble(tok[1], -2, 2), im = parseDouble(tok[2], -2, 2);
            if (!re || !im) { bad("c must be two numbers between -2 and 2"); return true; }
            st.s.juliaC = {*re, *im};
        } else if (tok.size() != 1) { bad("usage: j [re im]"); return true; }
        st.s.kind = Kind::Julia;
        st.v = defaultView(st.s.kind);
    } else if (c == "+" || c == "-" || c == "z") {
        double f = 2.0;
        if (tok.size() > 2) { bad("usage: + [factor]"); return true; }
        if (tok.size() == 2) {
            auto p = parseDouble(tok[1], 1.01, 1000);
            if (!p) { bad("factor must be a number in 1.01-1000"); return true; }
            f = *p;
        }
        double nw = (c == "-") ? st.v.halfWidth * f : st.v.halfWidth / f;
        if (nw < 1e-13) { bad("maximum zoom reached (double precision limit)"); return true; }
        if (nw > 100) { bad("already zoomed out as far as allowed"); return true; }
        st.v.halfWidth = nw;
    } else if (c == "w" || c == "a" || c == "s" || c == "d") {
        double f = 0.25;
        if (tok.size() > 2) { bad("usage: w|a|s|d [fraction]"); return true; }
        if (tok.size() == 2) {
            auto p = parseDouble(tok[1], 0.001, 10);
            if (!p) { bad("fraction must be a number in 0.001-10"); return true; }
            f = *p;
        }
        double step = f * st.v.halfWidth * 2;
        double vstep = step * (static_cast<double>(st.rows) / st.cols) * kTermAspect;
        if (c == "a") st.v.cx -= step;
        if (c == "d") st.v.cx += step;
        if (c == "w") st.v.cy += vstep;
        if (c == "s") st.v.cy -= vstep;
    } else if (c == "center") {
        if (tok.size() != 3) { bad("usage: center re im"); return true; }
        auto re = parseDouble(tok[1], -100, 100), im = parseDouble(tok[2], -100, 100);
        if (!re || !im) { bad("centre must be two numbers between -100 and 100"); return true; }
        st.v.cx = *re;
        st.v.cy = *im;
    } else if (c == "i") {
        auto n = tok.size() == 2 ? parseInt(tok[1], 1, 100000) : std::nullopt;
        if (!n) { bad("usage: i N with N in 1-100000"); return true; }
        st.s.maxIter = static_cast<int>(*n);
    } else if (c == "t") {
        auto n = tok.size() == 2 ? parseInt(tok[1], 0, 256) : std::nullopt;
        if (!n) { bad("usage: t N with N in 0-256"); return true; }
        st.threads = static_cast<unsigned>(*n);
    } else if (c == "size") {
        auto w = tok.size() == 3 ? parseInt(tok[1], 10, 300) : std::nullopt;
        auto h = tok.size() == 3 ? parseInt(tok[2], 5, 120) : std::nullopt;
        if (!w || !h) { bad("usage: size W H (10-300, 5-120)"); return true; }
        st.cols = static_cast<int>(*w);
        st.rows = static_cast<int>(*h);
    } else if (c == "color" || c == "colour") {
        st.colour = !st.colour;
    } else if (c == "r") {
        st.v = defaultView(st.s.kind);
    } else if (c == "e") {
        std::string file = "fractal.ppm";
        long w = 800, h = 600;
        if (tok.size() == 4) {
            file = tok[1];
            auto pw = parseInt(tok[2], 1, 8000), ph = parseInt(tok[3], 1, 8000);
            if (!pw || !ph) { bad("W and H must be integers in 1-8000"); return true; }
            w = *pw;
            h = *ph;
        } else if (tok.size() == 2) {
            file = tok[1];
        } else if (tok.size() != 1) { bad("usage: e [file W H]"); return true; }
        exportPPM(st, file, static_cast<int>(w), static_cast<int>(h));
        return true;
    } else if (c == "b") {
        bench::run(st.v, st.s, 800, 600, 3, "benchmark_results.csv");
        return true;
    } else if (c == "demo") {
        demo::Options o;
        o.threads = st.threads;
        demo::run(o);
        return true;
    } else if (c == "explore") {
        explore::Options o;
        o.threads = st.threads;
        explore::run(o);
        return true;
    } else {
        bad("unknown command '" + c + "'");
        return true;
    }
    draw(st);
    return true;
}

void usage() {
    std::cout <<
        "Fractal Explorer CLI\n\n"
        "Usage: fractal [options]       (no options = interactive mode)\n"
        "  --julia [re im]     use the Julia set (default c = -0.8 0.156)\n"
        "  --iter N            iteration limit (default 100)\n"
        "  --center re im      view centre        --zoom Z   zoom relative to default view\n"
        "  --threads N         worker threads (0 = all)\n"
        "  --size W H          print ASCII of this size and exit (default 78 28)\n"
        "  --export FILE [W H] write a PPM image (default 800 600) and exit\n"
        "  --bench             run the benchmark (800x600) and exit\n"
        "  --demo              animated slideshow of zooms and Julia morphs (Ctrl+C stops)\n"
        "  --explore           infinite zoom: Left = forward, Right = backward, Up/Down speed, Q quits\n"
        "  --slide-seconds S  seconds per slide in the demo (default 8)\n"
        "  --once              play the demo once instead of looping\n"
        "  --color             ANSI colour in ASCII output\n"
        "  --help\n";
}

} // namespace

int main(int argc, char** argv) {
    enableAnsi();
    State st;
    bool interactive = true, doBench = false, doExport = false, doDemo = false, doExplore = false;
    demo::Options demoOpt;
    explore::Options exploreOpt;
    std::string exportFile;
    int ew = 800, eh = 600;
    std::optional<double> zoom;
    std::optional<std::pair<double, double>> centre;

    auto fail = [](const std::string& m) {
        std::cerr << "Error: " << m << "\nTry --help.\n";
        return 2;
    };
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](int n) { return i + n < argc; };
        if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else if (a == "--julia") {
            st.s.kind = Kind::Julia;
            if (need(2)) {
                auto re = parseDouble(argv[i + 1], -2, 2), im = parseDouble(argv[i + 2], -2, 2);
                if (re && im) {
                    st.s.juliaC = {*re, *im};
                    i += 2;
                }
            }
        } else if (a == "--iter") {
            auto n = need(1) ? parseInt(argv[++i], 1, 100000) : std::nullopt;
            if (!n) return fail("--iter needs an integer in 1-100000");
            st.s.maxIter = static_cast<int>(*n);
        } else if (a == "--threads") {
            auto n = need(1) ? parseInt(argv[++i], 0, 256) : std::nullopt;
            if (!n) return fail("--threads needs an integer in 0-256");
            st.threads = static_cast<unsigned>(*n);
        } else if (a == "--zoom") {
            auto z = need(1) ? parseDouble(argv[++i], 1e-3, 1e12) : std::nullopt;
            if (!z) return fail("--zoom needs a positive number");
            zoom = *z;
        } else if (a == "--center") {
            auto re = need(2) ? parseDouble(argv[i + 1], -100, 100) : std::nullopt;
            auto im = need(2) ? parseDouble(argv[i + 2], -100, 100) : std::nullopt;
            if (!re || !im) return fail("--center needs two numbers");
            centre = std::make_pair(*re, *im);
            i += 2;
        } else if (a == "--size") {
            auto w = need(2) ? parseInt(argv[i + 1], 10, 300) : std::nullopt;
            auto h = need(2) ? parseInt(argv[i + 2], 5, 120) : std::nullopt;
            if (!w || !h) return fail("--size needs W (10-300) and H (5-120)");
            st.cols = static_cast<int>(*w);
            st.rows = static_cast<int>(*h);
            i += 2;
            interactive = false;
        } else if (a == "--export") {
            if (!need(1)) return fail("--export needs a file name");
            exportFile = argv[++i];
            doExport = true;
            interactive = false;
            if (need(2)) {
                auto w = parseInt(argv[i + 1], 1, 8000), h = parseInt(argv[i + 2], 1, 8000);
                if (w && h) {
                    ew = static_cast<int>(*w);
                    eh = static_cast<int>(*h);
                    i += 2;
                }
            }
        } else if (a == "--bench") {
            doBench = true;
            interactive = false;
        } else if (a == "--demo") {
            doDemo = true;
            interactive = false;
        } else if (a == "--explore") {
            doExplore = true;
            interactive = false;
        } else if (a == "--explore-frames") { // scripted smoke test: autoplay N frames then exit
            auto n = need(1) ? parseInt(argv[++i], 1, 100000) : std::nullopt;
            if (!n) return fail("--explore-frames needs an integer in 1-100000");
            exploreOpt.maxFrames = static_cast<int>(*n);
            exploreOpt.autoplay = true;
            doExplore = true;
            interactive = false;
        } else if (a == "--once") {
            demoOpt.loop = false;
        } else if (a == "--slide-seconds") {
            auto sec = need(1) ? parseDouble(argv[++i], 0.5, 600) : std::nullopt;
            if (!sec) return fail("--slide-seconds needs a number in 0.5-600");
            demoOpt.secondsPerSlide = *sec;
        } else if (a == "--color" || a == "--colour") {
            st.colour = true;
        } else {
            return fail("unknown option '" + a + "'");
        }
    }

    st.v = defaultView(st.s.kind);
    if (zoom) st.v.halfWidth /= *zoom;
    if (centre) {
        st.v.cx = centre->first;
        st.v.cy = centre->second;
    }

    if (doExplore) {
        exploreOpt.threads = st.threads;
        exploreOpt.colour = true;
        explore::run(exploreOpt);
        return 0;
    }
    if (doDemo) {
        demoOpt.threads = st.threads;
        demo::run(demoOpt);
        return 0;
    }
    if (doBench) {
        bench::run(st.v, st.s, 800, 600, 3, "benchmark_results.csv");
        return 0;
    }
    if (doExport) return exportPPM(st, exportFile, ew, eh) ? 0 : 1;
    if (!interactive) {
        draw(st);
        return 0;
    }

    std::cout << "=== Fractal Explorer CLI ===  (type h for commands)\n";
    draw(st);
    std::string line;
    for (;;) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (!handle(st, line)) break;
    }
    return 0;
}
