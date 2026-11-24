// Interactive "infinite" zoom: arrow keys fly into / out of one spot of the Mandelbrot set.
#pragma once

#include "fractal.hpp"

namespace explore {

struct Options {
    bool colour = true;
    unsigned threads = 0;
    int fps = 30;
    int maxFrames = 0;     // 0 = run until the user quits; >0 is used for scripted smoke tests
    bool autoplay = false; // start flying forward without a key held
};

// Deepest magnification, as log2(zoom), that plain doubles can still resolve.
double maxDepth();

// View and settings for a given depth (log2 of the magnification). Depth 0 is the
// classic whole-set picture; the view glides toward the target point as depth grows
// and the iteration limit grows with it. The mapping is a pure function of depth, so
// zooming out retraces exactly the same path.
void viewAtDepth(double depth, fractal::View& v, fractal::Settings& s);

void run(const Options& opt);

} // namespace explore
