# Part 1 - Core Engine (the hard part)

**Owner:** Safee Imran (25K-0980)
**Files:** `src/fractal.hpp`, `src/fractal.cpp`, `src/explore.hpp`, `src/explore.cpp`, `tests/test_core.cpp`

This is the heart of the project: the maths, the data structures, the threads and the recursion.

## What each file does

| File | Job |
|---|---|
| `fractal.hpp/.cpp` | `Grid` (vector storage), `escapeTime`, pixel-to-plane mapping, `renderSerial`, `renderParallel`, `renderSubdivision` |
| `explore.hpp/.cpp` | The interactive infinite zoom: key input, smooth velocity, depth-to-view mapping |
| `tests/test_core.cpp` | 11 point-by-point maths checks, threads == serial, subdivision accuracy, zoom path checks |

## The key ideas

1. **Escape-time algorithm.** Repeat `z = z*z + c`. If `|z| > 2` the point escapes and we record how many steps it took. If it has not escaped after `maxIter` steps, we treat it as inside the set.
   - Mandelbrot: `z0 = 0`, `c = the pixel`. Julia: `c` is fixed, `z0 = the pixel`.
   - We test `norm(z) > 4` instead of `abs(z) > 2` to avoid a square root (same answer, faster).
2. **`Grid` = one `std::vector<int>` used as a 2D array.** Pixel `(x, y)` lives at index `y * width + x` (row-major). One contiguous block is cache-friendly and needs no pointer-of-pointers.
3. **Multithreading (`renderParallel`).** Every pixel is independent, so this is an "embarrassingly parallel" problem. Threads grab the next unprocessed row from a shared `std::atomic<int>` counter (`fetch_add`).
   - Why not split into equal blocks? Rows near the set take far longer than rows far away, so equal blocks leave some threads idle. Dynamic row-grabbing balances the load.
   - No mutex is needed because each row is written by exactly one thread.
4. **Recursive subdivision (`renderSubdivision`).** Compute the border of a rectangle. If all border pixels have the same count, fill the interior without iterating. Otherwise split the longer side in half and recurse.
   - Grid starts at `-1` meaning "not computed", so shared borders are never computed twice.
   - It is a heuristic (a tiny island inside a uniform border could be missed): in the benchmark it computed 32% of pixels and 0.009% differed from the exact answer.
5. **Infinite zoom (`explore`).** `viewAtDepth(depth)` is a pure function: zoom = `2^depth`, so zooming out retraces exactly the same path. Iterations grow with depth (`100 + 75*depth`) so detail stays sharp. It stops at about 10^13x because `double` has ~16 digits and neighbouring pixels become identical.

## Likely viva questions

- **Why is the Mandelbrot set bounded by `|z| > 2`?** If `|z| > 2` (and `|c| <= 2`), then `|z|^2 + |c|` grows every step, so it is guaranteed to diverge.
- **What is the time complexity?** `O(W * H * maxIter)` worst case. Threads divide it by about the number of cores. Subdivision lowers the average, not the worst case.
- **Why is the speed-up 6.8x on 8 threads and not 8x?** Thread start-up cost, the final join, memory bandwidth, and some of the 8 "cores" are hyper-threads sharing hardware.
- **Why `std::atomic` and not a mutex?** One `fetch_add` is a single hardware instruction. A mutex would block threads and add overhead.
- **Is the parallel result identical to serial?** Yes, bit for bit. The tests check this for 1, 2, 3, 7 and "all" threads.
- **Why can't we zoom forever?** Doubles have 53 bits of precision. Below roughly 2e-13 width, two neighbouring pixels map to the same `double`. Fixing it needs perturbation theory or big-number arithmetic.
- **What is the pixel aspect ratio about?** Terminal characters are about twice as tall as wide, so `pixelToPlane` multiplies the vertical extent by 2 for terminal output (1 for images).

## Demo script (30 seconds)

```
fractal --bench            # shows speed-up table
fractal --explore          # hold Left to dive, Right to come back
```

## Run only this part's tests

```
build.bat test     (look for "Part 1: all tests passed")
```
