#pragma once

namespace strata::kernels {
// Legacy gfx1100 decode layouts. Opt-in: STRATA_LAB_RDNA_KQUANTS=1.
// Products match the native contract, but the summation order is different.
bool rdna_kquant_lab(int type, const void* w, const void* x, float* y,
                     int n_in, int n_out, int ncols, void* stream,
                     const void* w2 = nullptr, float* y2 = nullptr);
// Numeric probe only: -1 follows the environment, 0 disables, 1 enables.
void native_mmvq_rdna_kquant_probe_mode(int mode);
}
