#include "smooth.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>

namespace bf::dsp {

namespace {

// Compute median of a vector (modifies vector in-place or copies)
double compute_median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (v.size() % 2 != 0) {
        return v[mid];
    }
    double v_mid = v[mid];
    std::nth_element(v.begin(), v.begin() + mid - 1, v.end());
    return 0.5 * (v[mid - 1] + v_mid);
}

// Solve pentadiagonal symmetric positive-definite system A x = rhs
// where A has diagonal a[0..n-1], subdiagonal1 b[0..n-2], subdiagonal2 c[0..n-3]
bool solve_pentadiagonal(
    const std::vector<double>& a,
    const std::vector<double>& b,
    const std::vector<double>& c,
    const std::vector<double>& rhs,
    std::vector<double>& x
) {
    const size_t n = a.size();
    if (n == 0) return false;
    x.resize(n);

    if (n == 1) {
        if (a[0] <= 0.0) return false;
        x[0] = rhs[0] / a[0];
        return true;
    }
    if (n == 2) {
        double det = a[0] * a[1] - b[0] * b[0];
        if (det <= 0.0) return false;
        x[0] = (a[1] * rhs[0] - b[0] * rhs[1]) / det;
        x[1] = (a[0] * rhs[1] - b[0] * rhs[0]) / det;
        return true;
    }

    std::vector<double> l(n);
    std::vector<double> m(n - 1);
    std::vector<double> k(n - 2);

    // Cholesky decomposition L L^T
    // i = 0
    if (a[0] <= 0.0) return false;
    l[0] = std::sqrt(a[0]);
    m[0] = b[0] / l[0];
    k[0] = c[0] / l[0];

    // i = 1
    double l1_sq = a[1] - m[0] * m[0];
    if (l1_sq <= 0.0) return false;
    l[1] = std::sqrt(l1_sq);
    m[1] = (b[1] - m[0] * k[0]) / l[1];
    if (n > 3) {
        k[1] = c[1] / l[1];
    }

    // i = 2 .. n-1
    for (size_t i = 2; i < n; ++i) {
        double li_sq = a[i] - m[i - 1] * m[i - 1] - k[i - 2] * k[i - 2];
        if (li_sq <= 1e-12) {
            li_sq = 1e-12; // Numerical safety
        }
        l[i] = std::sqrt(li_sq);

        if (i + 1 < n) {
            m[i] = (b[i] - m[i - 1] * k[i - 1]) / l[i];
        }
        if (i + 2 < n) {
            k[i] = c[i] / l[i];
        }
    }

    // Forward substitution L y = rhs
    std::vector<double> y(n);
    y[0] = rhs[0] / l[0];
    y[1] = (rhs[1] - m[0] * y[0]) / l[1];
    for (size_t i = 2; i < n; ++i) {
        y[i] = (rhs[i] - m[i - 1] * y[i - 1] - k[i - 2] * y[i - 2]) / l[i];
    }

    // Back substitution L^T x = y
    x[n - 1] = y[n - 1] / l[n - 1];
    x[n - 2] = (y[n - 2] - m[n - 2] * x[n - 1]) / l[n - 2];
    for (int i = static_cast<int>(n) - 3; i >= 0; --i) {
        x[i] = (y[i] - m[i] * x[i + 1] - k[i] * x[i + 2]) / l[i];
    }

    return true;
}

} // namespace

Result<std::vector<double>> whittaker_huber_smooth(
    std::span<const double> detections,
    std::span<const double> weights,
    std::span<const double> breaks,
    const SmoothParams& params
) {
    const size_t n = detections.size();
    if (n == 0) {
        return Result<std::vector<double>>::error("Empty detections array in smoother");
    }

    std::vector<double> x(detections.begin(), detections.end());
    if (n < 3 || params.lambda <= 0.0) {
        // Strict mode (lambda == 0) or too few points for 2nd differences
        return Result<std::vector<double>>::ok(x);
    }

    // If lambda is infinite, do a weighted linear regression
    if (std::isinf(params.lambda) || params.lambda >= 1e12) {
        double sum_w = 0.0, sum_wi = 0.0, sum_wy = 0.0, sum_wii = 0.0, sum_wiy = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double w = (i < weights.size()) ? std::max(weights[i], 1e-4) : 1.0;
            double idx = static_cast<double>(i);
            double y = detections[i];
            sum_w += w;
            sum_wi += w * idx;
            sum_wy += w * y;
            sum_wii += w * idx * idx;
            sum_wiy += w * idx * y;
        }
        double denom = sum_w * sum_wii - sum_wi * sum_wi;
        if (std::abs(denom) > 1e-12) {
            double slope = (sum_w * sum_wiy - sum_wi * sum_wy) / denom;
            double intercept = (sum_wy - slope * sum_wi) / sum_w;
            for (size_t i = 0; i < n; ++i) {
                x[i] = intercept + slope * static_cast<double>(i);
            }
            return Result<std::vector<double>>::ok(x);
        }
    }

    // Precompute D^T * G * D matrix components
    // D is of size (n-2) x n with rows [1, -2, 1]
    std::vector<double> p_diag(n, 0.0);
    std::vector<double> p_super1(n - 1, 0.0);
    std::vector<double> p_super2(n - 2, 0.0);

    for (size_t j = 0; j < n - 2; ++j) {
        double g = (j < breaks.size()) ? breaks[j] : 1.0;
        if (g <= 0.0) continue; // Continuity broken at this change point

        p_diag[j]     += 1.0 * g;
        p_diag[j + 1] += 4.0 * g;
        p_diag[j + 2] += 1.0 * g;

        p_super1[j]     += -2.0 * g;
        p_super1[j + 1] += -2.0 * g;

        p_super2[j]     += 1.0 * g;
    }

    // Huber IRLS iterations
    std::vector<double> current_w(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        current_w[i] = (i < weights.size()) ? std::max(weights[i], 1e-4) : 1.0;
    }

    std::vector<double> a(n);
    std::vector<double> b(n - 1);
    std::vector<double> c(n - 2);
    std::vector<double> rhs(n);

    for (int iter = 0; iter < params.max_irls_iterations; ++iter) {
        // Assemble system: (W + lambda * P) x = W * S
        for (size_t i = 0; i < n; ++i) {
            a[i] = current_w[i] + params.lambda * p_diag[i];
            rhs[i] = current_w[i] * detections[i];
        }
        for (size_t i = 0; i < n - 1; ++i) {
            b[i] = params.lambda * p_super1[i];
        }
        for (size_t i = 0; i < n - 2; ++i) {
            c[i] = params.lambda * p_super2[i];
        }

        std::vector<double> next_x;
        if (!solve_pentadiagonal(a, b, c, rhs, next_x)) {
            return Result<std::vector<double>>::error("Pentadiagonal solver failed in whittaker_huber_smooth");
        }

        // Check convergence
        double max_change = 0.0;
        for (size_t i = 0; i < n; ++i) {
            max_change = std::max(max_change, std::abs(next_x[i] - x[i]));
        }
        x = std::move(next_x);

        if (max_change < params.convergence_tol) {
            break;
        }

        // Re-calculate Huber weights
        std::vector<double> abs_residuals(n);
        for (size_t i = 0; i < n; ++i) {
            abs_residuals[i] = std::abs(detections[i] - x[i]);
        }
        double mad = compute_median(abs_residuals);
        double s = 1.4826 * mad;
        if (s < 1e-6) s = 1e-6;

        for (size_t i = 0; i < n; ++i) {
            double base_w = (i < weights.size()) ? std::max(weights[i], 1e-4) : 1.0;
            double u = abs_residuals[i] / s;
            double huber_factor = (u <= params.huber_k) ? 1.0 : (params.huber_k / u);
            current_w[i] = std::max(base_w * huber_factor, 1e-4);
        }
    }

    return Result<std::vector<double>>::ok(x);
}

} // namespace bf::dsp
