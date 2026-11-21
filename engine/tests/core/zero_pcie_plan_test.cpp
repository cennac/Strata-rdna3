#include "strata/core/zero_pcie_plan.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace plan = strata::core::zero_pcie_plan;
constexpr int width = plan::max_rows * plan::top_k;
using IDs = std::array<int32_t, width>;
using Residency = std::array<int32_t, plan::experts>;
using Offsets = std::array<uint64_t, plan::experts>;

struct Output {
    int groups, entries;
    int32_t kind[width + 1], start[width + 2], dst[width + 1], tok[width + 1];
    unsigned long long ptr[width + 1];
};

void check(bool ok) {
    if (!ok) { std::fputs("zero_pcie_plan_test: check failed\n", stderr); std::abort(); }
}

void reference(const IDs& ids, int n, const Residency& res, const uint8_t* base,
               const uint64_t* offsets, Output& out) {
    int64_t distinct[width], first[width];
    int count = 0;
    for (int i = 0; i < n; ++i) {
        first[i] = plan::first_entry(ids.data(), i);
        if (first[i] == i) distinct[count++] = i;
    }
    out.groups = out.entries = 0;
    for (int q = 0; q < count; ++q) {
        const auto i0 = distinct[q];
        const int slot = res[ids[i0]];
        plan::each_entry(first, n, i0, [&](int64_t i) { out.kind[i] = slot < 0 ? -1 : 0; });
        if (slot < 0) continue;
        out.ptr[out.groups] = plan::resident_ptr(base, offsets, slot, 64);
        out.start[out.groups++] = out.entries;
        plan::each_entry(first, n, i0, [&](int64_t i) {
            out.dst[out.entries] = (int32_t) i;
            out.tok[out.entries++] = (int32_t) (i / plan::top_k);
        });
    }
}

bool candidate(const IDs& ids, int n, const Residency& res, const uint8_t* base,
               const uint64_t* offsets, Output& out) {
    return plan::build(ids.data(), n, res.data(), base, offsets, 64,
                       out.kind, out.ptr, out.start, out.dst, out.tok, out.groups, out.entries);
}

IDs routes(std::mt19937& rng, int n, bool repeated) {
    IDs ids{};
    const unsigned e0 = rng() % plan::experts;
    for (int i = 0; i < n; ++i) {
        int e;
        do {
            const auto r = rng();
            e = repeated && i >= plan::top_k && (r & 3u) != 0
                ? (int) ((e0 + r % 16) % plan::experts) : (int) (r % plan::experts);
        } while (std::find(ids.begin() + i / plan::top_k * plan::top_k, ids.begin() + i, e) != ids.begin() + i);
        ids[i] = e;
    }
    return ids;
}

Residency residency(const IDs& ids, int n, int misses) {
    Residency res;
    for (int e = 0; e < plan::experts; ++e) res[e] = plan::experts - 1 - e;
    for (int i = 0; i < n && misses != 0; ++i) {
        if (res[ids[i]] < 0) continue;
        res[ids[i]] = -1;
        --misses;
    }
    return res;
}

volatile uint64_t checksum = 0;

// Indirect call includes the whole builder, including index initialization. Inputs
// are hot and fixed within an A/B; no claim is made about live GPU or mapped writes.
template <typename Builder>
double sample(Builder build, const std::vector<IDs>& ids, const std::vector<Residency>& res,
              int n, const uint8_t* base, const uint64_t* offsets) {
    constexpr int repeats = 128;
    uint64_t sum = 0;
    Output out{};
    const auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < repeats; ++rep)
        for (std::size_t i = 0; i < ids.size(); ++i) {
            build(ids[i], n, res[i], base, offsets, out);
            sum += out.entries + out.groups + 1 + out.kind[rep % n];
        }
    const auto t1 = std::chrono::steady_clock::now();
    checksum = checksum + sum;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (repeats * ids.size());
}

int main(int argc, char**) {
    check(!plan::supported(1, 10, 512, 40) && !plan::supported(5, 10, 512, 50));
    check(!plan::supported(4, 9, 512, 40) && !plan::supported(4, 10, 256, 40));
    check(!plan::supported(4, 10, 512, 39) && plan::supported(4, 10, 512, 40));
    std::mt19937 rng(42);
    static const uint8_t base[plan::experts * 128] = {};
    Offsets offsets;
    for (int e = 0; e < plan::experts; ++e) offsets[e] = (uint64_t) e * 128;
    int checks = 0;
    for (int rows = 2; rows <= 4; ++rows)
        for (int trial = 0; trial < 256; ++trial) {
            const int n = rows * plan::top_k;
            auto ids = routes(rng, n, trial % 2 != 0);
            for (int misses : {0, 1, 2, n})
                for (const uint64_t* off : std::array<const uint64_t*, 2>{nullptr, offsets.data()}) {
                    auto res = residency(ids, n, misses);
                    Output expected, got;
                    std::memset(&expected, 0x5a, sizeof expected);
                    std::memset(&got, 0x5a, sizeof got);
                    reference(ids, n, res, base, off, expected);
                    check(candidate(ids, n, res, base, off, got));
                    check(std::memcmp(&expected, &got, sizeof got) == 0);
                    ++checks;
                }
            const auto res = residency(ids, n, 0);
            ids[n - 1] = trial % 2 == 0 ? -1 : plan::experts;
            Output rejected, untouched;
            std::memset(&rejected, 0x5a, sizeof rejected);
            std::memset(&untouched, 0x5a, sizeof untouched);
            check(!candidate(ids, n, res, base, nullptr, rejected));
            check(std::memcmp(&rejected, &untouched, sizeof rejected) == 0);
            ++checks;
        }
    std::printf("zero_pcie_plan_test: %d checks passed\n", checks);
    if (argc < 2) return 0;
    for (int rows : {2, 3, 4})
        for (bool repeated : {false, true})
            for (int misses : {0, 1, 2, rows * plan::top_k}) {
                const int n = rows * plan::top_k;
                std::vector<IDs> ids;
                std::vector<Residency> res;
                for (int i = 0; i < 256; ++i) {
                    ids.push_back(routes(rng, n, repeated));
                    res.push_back(residency(ids.back(), n, misses));
                }
                std::array<double, 5> a{}, b{};
                for (int i = 0; i < 5; ++i) {
                    auto old = [&] { a[i] = sample(reference, ids, res, n, base, offsets.data()); };
                    auto fast = [&] { b[i] = sample(candidate, ids, res, n, base, offsets.data()); };
                    if (i % 2) { fast(); old(); } else { old(); fast(); }
                }
                std::sort(a.begin(), a.end());
                std::sort(b.begin(), b.end());
                std::printf("rows=%d repeated=%d misses=%d reference=%.1f candidate=%.1f ns/plan\n",
                            rows, (int) repeated, misses, a[2], b[2]);
            }
}
