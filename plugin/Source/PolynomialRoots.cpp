// The roots of a polynomial: PolynomialRoots.h.  Moved whole from SourceFilterModel::shiftFormants in v176.
#include "PolynomialRoots.h"
#include <cmath>

namespace VSE {

namespace {

using Poly = std::vector<std::complex<double>>;

// One Laguerre step from x on the polynomial p of degree deg (complex polynomial, complex iterate)
std::complex<double> laguerreStep(const Poly& p, int deg, std::complex<double> x) {
    std::complex<double> pv = p[0], dp = 0.0, d2p = 0.0;
    for (int k = 1; k <= deg; ++k) {
        d2p = d2p * x + dp;
        dp  = dp  * x + pv;
        pv  = pv  * x + p[k];
    }
    d2p *= 2.0;
    if (std::abs(pv) < 1e-30) return x;
    const std::complex<double> G   = dp / pv;
    const std::complex<double> H   = G * G - d2p / pv;
    const std::complex<double> sq  = std::sqrt(std::complex<double>(deg - 1) *
                                                (std::complex<double>(deg) * H - G * G));
    const std::complex<double> d1  = G + sq;
    const std::complex<double> d2  = G - sq;
    const std::complex<double> den = (std::abs(d1) >= std::abs(d2)) ? d1 : d2;
    if (std::abs(den) < 1e-30) return x;
    return x - std::complex<double>(deg) / den;
}

// Laguerre steps from x on p until one moves less than `tol` of 1 + |x|, at most `steps` of them
std::complex<double> converge(const Poly& p, int deg, std::complex<double> x, int steps, double tol) {
    for (int iter = 0; iter < steps; ++iter) {
        const std::complex<double> xnew = laguerreStep(p, deg, x);
        const double ch = std::abs(xnew - x);
        x = xnew;
        if (ch < tol * (1.0 + std::abs(x))) break;
    }
    return x;
}

} // namespace

std::vector<std::complex<double>> polynomialRoots(const std::vector<std::complex<double>>& p) {
    const int n = static_cast<int>(p.size()) - 1;
    std::vector<std::complex<double>> roots;
    roots.reserve(n);
    Poly deflated = p;
    for (int i = n; i >= 1; --i) {
        std::complex<double> x(0.2 + 0.07 * double(i), 0.3 - 0.05 * double(i));
        x = converge(deflated, i, x, 200, 1e-12);
        x = converge(p, n, x, 60, 1e-14);   // polished on the full polynomial, the deflation's error removed
        roots.push_back(x);
        // Single-root Horner deflation
        Poly q(i);
        q[0] = deflated[0];
        for (int k = 1; k < i; ++k)
            q[k] = deflated[k] + x * q[k - 1];
        deflated = q;
    }
    return roots;
}

} // namespace VSE
