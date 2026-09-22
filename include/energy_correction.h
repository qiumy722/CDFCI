#ifndef CDFCI_ENERGY_CORRECTION_H
#define CDFCI_ENERGY_CORRECTION_H

#include "config.h"
#include <algorithm>
#include <cmath>

// Energy-only projected diagonal/Olsen correction (Algorithm 1, Eqs. 15-19
// in cdfci_true_perturbation.pdf). No H application, determinant insertion,
// coefficient update, diagonal cache, or correction vector is needed.
//
// For unnormalised x, put a_i = (z_p/x_p) x_i - z_i, m_i = H_ii - E.
// With A=sum(a_i^2/m_i), B=sum(a_i*x_i/m_i), C=sum(x_i^2/m_i), i != p,
// delta_internal = (-A + m_p*B^2/(x_p^2 + m_p*C)) / (x'x).
// x_i=0 entries affect only A, so their contribution can be reported separately
// as the predecessor's stored-space external PT2. Compressed external z entries
// give an approximate external correction, not full-space EN-PT2.
template <typename H, typename W>
EnergyCorrectionResult compute_energy_correction(const H &ham, const W &wf,
                                                 bool include_external = true,
                                                 NumericalType gap_tolerance = 1e-12)
{
    static_assert(W::NSTATES_val == 1, "Energy correction currently supports one state");
    using Quad = QUAD_PRECISION;
    using Key = typename W::key_type;
    EnergyCorrectionResult out;
    out.scope = include_external ? "stored" : "internal";
    const Quad norm = wf.get_xx();
    if (!(norm > 0) || !std::isfinite(static_cast<double>(norm))) {
        out.status = "invalid_norm";
        return out;
    }
    const Quad energy = wf.get_xz() / norm;
    out.variational_energy = static_cast<double>(energy);
    if (!std::isfinite(out.variational_energy)) {
        out.status = "nonfinite_energy";
        return out;
    }

    Key pivot{};
    NumericalType xp = 0, zp = 0;
    bool finite = true;
    // Select a large coefficient without evaluating any diagonals.
    wf.loop([&](const auto &entry) {
        auto x = entry.second[0], z = entry.second[1];
        finite = finite && std::isfinite(x) && std::isfinite(z);
        if (std::abs(x) > std::abs(xp)) {
            pivot = entry.first;
            xp = x;
            zp = z;
        }
    });
    if (!finite || xp == 0) {
        out.status = "invalid_coefficients";
        return out;
    }

    // Normally one diagonal pass suffices. If a nonpivot internal diagonal
    // is zero/negative (or numerically close), eliminate it and retry once.
    // Two such coordinates cannot satisfy the projected positivity condition.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const double dp = ham.get_diagonal(pivot);
        ++out.diagonal_evaluations;
        if (!std::isfinite(dp)) {
            out.status = "nonfinite_diagonal";
            return out;
        }
        const Quad mp = static_cast<Quad>(dp) - energy;
        const Quad ratio = static_cast<Quad>(zp) / xp;
        Quad aa = 0, ax = 0, xx = 0, external = 0;
        Quad residual_internal = 0, residual_external = 0;
        size_t bad_internal = 0;
        bool external_ok = true, all_finite = true;
        Key replacement{};
        double replacement_x = 0, replacement_z = 0;
        wf.loop([&](const auto &entry) {
            const double x = entry.second[0], z = entry.second[1];
            if (x == 0 && (z == 0 || !include_external)) return;
            const Quad residual = energy * x - z;
            if (x != 0) residual_internal += residual * residual;
            else residual_external += residual * residual;
            if (typename W::key_equal{}(entry.first, pivot)) return;
            auto det = entry.first;
            const double diagonal = ham.get_diagonal(det);
            ++out.diagonal_evaluations;
            all_finite = all_finite && std::isfinite(diagonal);
            const Quad gap = static_cast<Quad>(diagonal) - energy;
            const double tolerance = gap_tolerance *
                std::max({1.0, std::abs(diagonal), std::abs(out.variational_energy)});
            if (!(gap > tolerance)) {
                if (x != 0) {
                    ++bad_internal;
                    replacement = det;
                    replacement_x = x;
                    replacement_z = z;
                } else external_ok = false;
                return;
            }
            if (x == 0) {
                external -= static_cast<Quad>(z) * z / gap;
            } else {
                const Quad a = ratio * x - z;
                aa += a * a / gap;
                ax += a * x / gap;
                xx += static_cast<Quad>(x) * x / gap;
            }
        });
        if (!all_finite) {
            out.status = "nonfinite_diagonal";
            return out;
        }
        if (bad_internal == 1 && attempt == 0) {
            pivot = replacement;
            xp = replacement_x;
            zp = replacement_z;
            continue;
        }
        out.internal_residual_norm = static_cast<double>(sqrt(residual_internal / norm));
        out.stored_residual_norm = static_cast<double>(sqrt((residual_internal + residual_external) / norm));
        out.external_valid = external_ok;
        if (external_ok) out.external_correction = static_cast<double>(external / norm);
        if (bad_internal != 0) {
            out.status = "unsafe_internal_diagonal_gap";
            return out;
        }
        const Quad pivot_square = static_cast<Quad>(xp) * xp;
        const Quad denominator = pivot_square + mp * xx;
        if (!(denominator > gap_tolerance * (pivot_square + fabs(mp * xx)))) {
            out.status = "singular_or_indefinite_projected_diagonal";
            return out;
        }
        const Quad internal = (-aa + mp * ax * ax / denominator) / norm;
        out.internal_correction = static_cast<double>(internal);
        out.internal_valid = std::isfinite(out.internal_correction);
        out.valid = out.internal_valid && external_ok && std::isfinite(out.external_correction);
        if (out.valid) {
            out.corrected_energy = static_cast<double>(energy + internal + external / norm);
            out.valid = std::isfinite(out.corrected_energy);
        }
        out.status = out.valid ? "ok" : (external_ok ? "nonfinite_correction" : "unsafe_external_diagonal_gap");
        return out;
    }
    return out;
}

#endif
