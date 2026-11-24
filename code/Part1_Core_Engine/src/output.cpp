#include "output.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace output
{

    RGB colourFor(int n, int maxIter)
    {
        if (n >= maxIter)
            return {0, 0, 0}; // inside the set
        double t = std::sqrt(static_cast<double>(n) / maxIter);
        t = std::clamp(t, 0.0, 1.0);
        // Bernstein-polynomial gradient: deep blue -> cyan -> yellow -> orange.
        double r = 9.0 * (1 - t) * t * t * t;
        double g = 15.0 * (1 - t) * (1 - t) * t * t;
        double b = 8.5 * (1 - t) * (1 - t) * (1 - t) * t;
        auto c = [](double v)
        { return static_cast<unsigned char>(std::clamp(v, 0.0, 1.0) * 255.0); };
        return {c(r * 1.6), c(g * 1.6), c(b * 1.6 + 0.04)};
    }

    std::string toAscii(const fractal::Grid &g, int maxIter, bool colour)
    {
        static const std::string ramp = ".,:;+=*%#@";
        std::ostringstream out;
        for (int y = 0; y < g.height(); ++y)
        {
            for (int x = 0; x < g.width(); ++x)
            {
                int n = g.at(x, y);
                if (n >= maxIter)
                {
                    out << ' ';
                    continue;
                }
                double t = std::sqrt(static_cast<double>(n) / maxIter);
                int idx = std::min<int>(static_cast<int>(ramp.size()) - 1, static_cast<int>(t * ramp.size()));
                if (colour)
                {
                    RGB c = colourFor(n, maxIter);
                    // Map to the 6x6x6 colour cube (codes 16..231).
                    int code = 16 + 36 * (c.r * 5 / 255) + 6 * (c.g * 5 / 255) + (c.b * 5 / 255);
                    out << "\x1b[38;5;" << code << 'm' << ramp[idx];
                }
                else
                {
                    out << ramp[idx];
                }
            }
            if (colour)
                out << "\x1b[0m";
            out << '\n';
        }
        return out.str();
    }

    std::string toAsciiBands(const fractal::Grid &g, int maxIter, int period, bool colour)
    {
        static const std::string ramp = ".,:;+=*%#@";
        std::ostringstream out;
        for (int y = 0; y < g.height(); ++y)
        {
            for (int x = 0; x < g.width(); ++x)
            {
                int n = g.at(x, y);
                if (n >= maxIter)
                {
                    out << ' ';
                    continue;
                }
                // Triangle wave 0..1..0 over `period` iterations: no hard jump at the wrap.
                double phase = static_cast<double>(n % period) / period;
                double t = 1.0 - std::abs(2.0 * phase - 1.0);
                int idx = std::min<int>(static_cast<int>(ramp.size()) - 1, static_cast<int>(t * ramp.size()));
                if (colour)
                {
                    RGB c = colourFor(static_cast<int>(t * 1000), 1001);
                    int code = 16 + 36 * (c.r * 5 / 255) + 6 * (c.g * 5 / 255) + (c.b * 5 / 255);
                    out << "\x1b[38;5;" << code << 'm' << ramp[idx];
                }
                else
                {
                    out << ramp[idx];
                }
            }
            if (colour)
                out << "\x1b[0m";
            out << '\n';
        }
        return out.str();
    }

    void terminalSize(int &cols, int &rows)
    {
        cols = 80;
        rows = 24;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        {
            cols = info.srWindow.Right - info.srWindow.Left + 1;
            rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        }
#else
        struct winsize ws;
        if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
        {
            cols = ws.ws_col;
            rows = ws.ws_row;
        }
#endif
    }

    bool writePPM(const std::string &path, const fractal::Grid &g, int maxIter, std::string &err)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f)
        {
            err = "cannot open '" + path + "' for writing";
            return false;
        }
        f << "P6\n"
          << g.width() << ' ' << g.height() << "\n255\n";
        std::vector<char> row(static_cast<std::size_t>(g.width()) * 3);
        for (int y = 0; y < g.height(); ++y)
        {
            for (int x = 0; x < g.width(); ++x)
            {
                RGB c = colourFor(g.at(x, y), maxIter);
                row[x * 3] = static_cast<char>(c.r);
                row[x * 3 + 1] = static_cast<char>(c.g);
                row[x * 3 + 2] = static_cast<char>(c.b);
            }
            f.write(row.data(), static_cast<std::streamsize>(row.size()));
        }
        if (!f)
        {
            err = "write error on '" + path + "'";
            return false;
        }
        return true;
    }

} // namespace output
