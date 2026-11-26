// Self-running animated slideshow: zooms and morphs fractals in the terminal.
#pragma once

namespace demo {

struct Options {
    double secondsPerSlide = 8.0;
    int fps = 20;
    bool colour = true;
    bool loop = true; // repeat until Ctrl+C
    unsigned threads = 0;
};

// Runs the slideshow (Ctrl+C stops it). Returns when stopped or finished.
void run(const Options& opt);

} // namespace demo
