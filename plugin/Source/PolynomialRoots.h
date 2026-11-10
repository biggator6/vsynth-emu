#pragma once

#include <complex>
#include <vector>

namespace VSE {

// The n roots of p[0]·zⁿ + p[1]·zⁿ⁻¹ + … + p[n], by Laguerre's method on the deflated polynomial, each root then
// polished on the whole one: single-root Horner deflation accumulates error, and the polish removes it (the deflated
// polynomial only steers the next search).  SourceFilterModel's full-rate formant shift and DownsampledFormants both
// find their poles with it (v176; until then each had its own copy).
// Press et al., "Numerical Recipes in C++", 3rd ed., §9.5 (Laguerre's Method).
std::vector<std::complex<double>> polynomialRoots(const std::vector<std::complex<double>>& p);

} // namespace VSE
