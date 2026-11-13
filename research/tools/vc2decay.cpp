// A reverb's decay (vc2decay.h).
#include "vc2decay.h"
#include <algorithm>

namespace vc2 {

namespace {

constexpr size_t kWindow = 441;          // 10 ms at 44.1 kHz
constexpr double kAboveFloor = 10;       // the cut: a window within 10 dB of the floor
constexpr double kTop = -5, kT30 = -35, kT20 = -25;
constexpr double kSettledDb = 3;         // the last tenth's halves this close: the tail has met its floor

double meanOf(const std::vector<double>& e, size_t from, size_t to) {
    double s = 0;
    for (size_t t = from; t < to; ++t) s += e[t];
    return to > from ? s / (double)(to - from) : 0;
}

// The least-squares slope of the integral's dB against the sample, over its points from kTop down to `bottom`
double slopeOf(const std::vector<double>& db, double bottom) {
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t t = 0; t < db.size(); ++t) {
        if (db[t] > kTop || db[t] < bottom) continue;
        n += 1;
        sx += (double)t;
        sy += db[t];
        sxx += (double)t * (double)t;
        sxy += (double)t * db[t];
    }
    return n > 2 ? (n * sxy - sx * sy) / (n * sxx - sx * sx) : 0;
}

} // namespace

std::vector<double> lessDc(const std::vector<int32_t>& x) {
    const size_t from = x.size() - x.size() / 10;
    double dc = 0;
    for (size_t t = from; t < x.size(); ++t) dc += x[t];
    dc /= (double)(x.size() - from);
    std::vector<double> y(x.size());
    for (size_t t = 0; t < x.size(); ++t) y[t] = x[t] - dc;
    return y;
}

Decay decayOf(const std::vector<double>& l, const std::vector<double>& r, double rate, double reference) {
    const size_t n = l.size();
    std::vector<double> e(n);
    for (size_t t = 0; t < n; ++t) e[t] = l[t] * l[t] + r[t] * r[t];
    const double floor = meanOf(e, n - n / 10, n);
    const double early = meanOf(e, n - n / 10, n - n / 20), late = meanOf(e, n - n / 20, n);
    size_t loudest = 0;
    double most = 0;
    for (size_t w = 0; w + kWindow <= n; w += kWindow) {
        const double m = meanOf(e, w, w + kWindow);
        if (m > most) loudest = w;
        most = std::max(most, m);
    }
    size_t end = n;
    for (size_t w = loudest; w + kWindow <= n && end == n; w += kWindow)
        if (meanOf(e, w, w + kWindow) < floor * std::pow(10, kAboveFloor / 10)) end = w;
    std::vector<double> edc(end + 1, 0);
    for (size_t t = end; t-- > 0;) edc[t] = edc[t + 1] + std::max(0.0, e[t] - floor);
    Decay d;
    d.settled = late > 0 ? std::abs(10 * std::log10(early / late)) < kSettledDb : early <= 0;
    d.rangeDb = floor > 0 ? 10 * std::log10(most / floor) : INFINITY;
    d.floorDb = floor > 0 ? 10 * std::log10(floor / reference) : -INFINITY;
    if (edc[0] <= 0) return d;
    std::vector<double> db(end);
    for (size_t t = 0; t < end; ++t) db[t] = edc[t] > 0 ? 10 * std::log10(edc[t] / edc[0]) : -999;
    const double lowest = end ? *std::min_element(db.begin(), db.end()) : 0;
    d.span = lowest < kT30 ? 30 : lowest < kT20 ? 20 : 0;
    const double slope = d.span ? slopeOf(db, d.span == 30 ? kT30 : kT20) : 0;
    if (slope < 0) d.seconds = -60 / slope / rate;
    return d;
}

} // namespace vc2
