// Turning escape-count grids into ASCII art and PPM images.
#pragma once
#include <string>

#include "fractal.hpp"

namespace output
{

    // Multi-line ASCII picture. With colour, 256-colour ANSI escapes are added.
    std::string toAscii(const fractal::Grid &g, int maxIter, bool colour);

    // Like toAscii, but shading and colour repeat (as a triangle wave) every `period`
    // iterations, so very deep zooms keep visible banding instead of one flat colour.
    std::string toAsciiBands(const fractal::Grid &g, int maxIter, int period, bool colour);

    // Terminal size in character cells (80x24 if it cannot be detected).
    void terminalSize(int &cols, int &rows);

    // Writes a binary PPM (P6). Returns false (and sets err) on failure.
    bool writePPM(const std::string &path, const fractal::Grid &g, int maxIter, std::string &err);

    struct RGB
    {
        unsigned char r, g, b;
    };
    RGB colourFor(int n, int maxIter);

} // namespace output
