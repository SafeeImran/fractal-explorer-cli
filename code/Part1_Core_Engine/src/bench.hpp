// Benchmark: single- vs multi-threaded rendering and subdivision.
#pragma once
#include <string>

#include "fractal.hpp"

namespace bench
{

    // Prints a table to stdout; writes CSV to csvPath if non-empty.
    void run(const fractal::View &v, const fractal::Settings &s, int width, int height, int reps,
             const std::string &csvPath);

} // namespace bench
