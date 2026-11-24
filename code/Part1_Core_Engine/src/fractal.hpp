// Core fractal maths: escape-time iteration, pixel grid and renderers.
#pragma once
#include <complex>
#include <cstddef>
#include <vector>

namespace fractal {

enum class Kind { Mandelbrot, Julia };

using Complex = std::complex<double>;

// Escape counts for every pixel, stored row-major in one contiguous vector.
class Grid {
public:
    Grid(int w, int h, int fill = 0) : w_(w), h_(h), data_(static_cast<std::size_t>(w) * h, fill) {}
    int width() const { return w_; }
    int height() const { return h_; }
    int& at(int x, int y) { return data_[static_cast<std::size_t>(y) * w_ + x]; }
    int at(int x, int y) const { return data_[static_cast<std::size_t>(y) * w_ + x]; }
    const std::vector<int>& data() const { return data_; }
    void fill(int v) { data_.assign(data_.size(), v); }

private:
    int w_, h_;
    std::vector<int> data_;
};

// Centre of the view and half of its horizontal extent in the complex plane.
struct View {
    double cx = -0.75;
    double cy = 0.0;
    double halfWidth = 1.75;
};

struct Settings {
    Kind kind = Kind::Mandelbrot;
    int maxIter = 100;
    Complex juliaC{-0.8, 0.156};
};

View defaultView(Kind kind);
const char* kindName(Kind kind);

// Iterates z -> z^2 + c from z0. Returns the number of iterations performed when
// |z| first exceeds 2, or maxIter if the point never escapes (treated as inside).
int escapeTime(Complex z0, Complex c, int maxIter);

// Escape time of the plane point p under the chosen fractal.
int pixelValue(const Settings& s, Complex p);

// Maps pixel (x, y) of a w*h grid to the complex plane. cellAspect is the height of
// one cell divided by its width (about 2 for terminal characters, 1 for images).
Complex pixelToPlane(const View& v, int x, int y, int w, int h, double cellAspect);

// Baseline renderer: one thread, every pixel evaluated.
void renderSerial(Grid& g, const View& v, const Settings& s, double cellAspect);

// Rows are handed out dynamically to `threads` workers (0 = hardware concurrency).
void renderParallel(Grid& g, const View& v, const Settings& s, double cellAspect, unsigned threads);

// Optional extension: recursive rectangle subdivision. If the whole border of a
// rectangle has the same escape count, the interior is filled without iterating.
// Heuristic (exact only when the region has no hidden islands); single-threaded.
// Returns the number of pixels actually evaluated.
std::size_t renderSubdivision(Grid& g, const View& v, const Settings& s, double cellAspect);

unsigned defaultThreads();

} // namespace fractal
