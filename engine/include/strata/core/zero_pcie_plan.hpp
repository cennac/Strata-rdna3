#pragma once

#include <cstddef>
#include <cstdint>

namespace strata::core::zero_pcie_plan {

constexpr int experts = 512;
constexpr int top_k = 10;
constexpr int max_rows = 4;

inline bool supported(int64_t rows, int64_t k, int64_t ne, int64_t cap) {
    return rows >= 2 && rows <= max_rows && k == top_k && ne == experts && cap >= rows * k;
}

// The general planner uses these scans for PCIe and helper tiers as well. Keep the
// reference operations here so the CPU probe uses the same scans and address rule.
inline int64_t first_entry(const int32_t* ids, int64_t i) {
    for (int64_t j = 0; j < i; ++j)
        if (ids[j] == ids[i]) return j;
    return i;
}

template <typename F>
inline void each_entry(const int64_t* first, int64_t n, int64_t i0, F&& f) {
    for (int64_t i = i0; i < n; ++i)
        if (first[i] == i0) f(i);
}

inline unsigned long long resident_ptr(const uint8_t* base, const uint64_t* offsets,
                                       int32_t slot, std::size_t stride) {
    return (unsigned long long) (base + (offsets ? (std::size_t) offsets[slot] : (std::size_t) slot * stride));
}

// Only called for supported shapes. Reject invalid IDs before touching any output.
// Groups and their rows are emitted in the general planner's original order.
inline bool build(const int32_t* ids, int n, const int32_t* resident,
                  const uint8_t* base, const uint64_t* offsets, std::size_t stride,
                  int32_t* kind, unsigned long long* ptr, int32_t* start,
                  int32_t* dst, int32_t* tok, int& groups, int& entries) {
    constexpr int cap = max_rows * top_k;
    uint8_t group_of[experts] = {};   // one-based; 0 means this expert has not appeared yet
    uint8_t first[cap], last[cap], next[cap];
    int distinct = 0;
    for (int i = 0; i < n; ++i) {
        const unsigned e = (unsigned) ids[i];
        if (e >= experts) return false;
        const int seen = group_of[e];
        if (seen == 0) {
            first[distinct] = last[distinct] = (uint8_t) i;
            group_of[e] = (uint8_t) ++distinct;
        } else {
            next[last[seen - 1]] = (uint8_t) i;
            last[seen - 1] = (uint8_t) i;
        }
        next[i] = (uint8_t) n;
    }
    groups = entries = 0;
    for (int q = 0; q < distinct; ++q) {
        const int32_t slot = resident[ids[first[q]]];
        if (slot < 0) {
            for (int i = first[q]; i < n; i = next[i]) kind[i] = -1;
            continue;
        }
        ptr[groups] = resident_ptr(base, offsets, slot, stride);
        start[groups++] = entries;
        for (int i = first[q]; i < n; i = next[i]) {
            kind[i] = 0;
            dst[entries] = i;
            tok[entries++] = i / top_k;
        }
    }
    return true;   // caller writes the terminal offsets and publishes the plan
}

}  // namespace strata::core::zero_pcie_plan
