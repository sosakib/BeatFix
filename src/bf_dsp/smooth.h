#pragma once

#include "result.h"
#include <vector>
#include <span>

namespace bf::dsp {

struct SmoothParams {
    double lambda = 2.914213562; // Default Natural (N_c = 8)
    int max_irls_iterations = 5;
    double huber_k = 1.345;
    double convergence_tol = 1e-6; // In seconds
};

// Solve (W + lambda * D^T * G * D) x = W * S
// with Huber IRLS re-weighting
Result<std::vector<double>> whittaker_huber_smooth(
    std::span<const double> detections,
    std::span<const double> weights,      // Confidence w_i in [0, 1]
    std::span<const double> breaks,       // Continuity g_i in {0, 1}, size n-2
    const SmoothParams& params = SmoothParams{}
);

} // namespace bf::dsp
