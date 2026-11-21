// Bitwise parity for PLE history advance, snapshots, and verifier-style rollback. No model needed.
#include <hip/hip_runtime.h>

#include "strata/kernels/elementwise.hpp"
#include "strata/kernels/ngram.hpp"
#include "strata/kernels/ple.hpp"
#include "strata/kernels/verify_kernels.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace k = strata::kernels;

using Words = std::vector<uint32_t>;
using Rows = std::vector<int>;
constexpr size_t kChannels = k::NG_HC_DIM;
constexpr size_t kDepth = k::NG_HIST;
constexpr size_t kHistory = kChannels * kDepth;
constexpr size_t kGuard = 32;
constexpr uint32_t kSentinel = 0xdeadbeefu;
constexpr int kReplays = 3;
static_assert(sizeof(uint32_t) == sizeof(float) && kHistory % 4 == 0);

// Live histories and normalized rows are tightly packed. Snapshots have one unused
// row on either side of the compact verifier window, in addition to the word guards.
size_t hist_offset(size_t slot) { return kGuard + slot * kHistory; }
size_t norm_offset(size_t row) { return kGuard + row * kChannels; }
size_t snap_offset(size_t row) { return kGuard + (row + 1) * kHistory; }

void hip_check(hipError_t status, const char* call) {
    if (status != hipSuccess) throw std::runtime_error(std::string(call) + ": " + hipGetErrorString(status));
}
#define HIP_CHECK(call) hip_check((call), #call)

struct Buffer {
    void* ptr = nullptr;
    explicit Buffer(size_t words) { HIP_CHECK(hipMalloc(&ptr, words * sizeof(uint32_t))); }
    ~Buffer() { (void) hipFree(ptr); }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    float* floats() const { return static_cast<float*>(ptr); }
    void upload(const Words& data, hipStream_t stream) const {
        HIP_CHECK(hipMemcpyAsync(ptr, data.data(), data.size() * sizeof(uint32_t), hipMemcpyHostToDevice, stream));
    }
    Words read(size_t words) const {
        Words data(words);
        HIP_CHECK(hipMemcpy(data.data(), ptr, words * sizeof(uint32_t), hipMemcpyDeviceToHost));
        return data;
    }
};

uint32_t pattern(size_t i, uint32_t seed) {
    // Construct bits on the host without evaluating any of these as floats.
    constexpr uint32_t special[] = {
        0u, 0x80000000u, 0x7f800000u, 0xff800000u, // signed zero and infinity
        0x00000001u, 0x807fffffu, 0x00800000u, 0x7f7fffffu, // subnormal and finite edges
        0x7fc12345u, 0xffc54321u, 0x7fa13579u, 0xffa00001u, // quiet and signaling NaN payloads
    };
    constexpr size_t n = sizeof(special) / sizeof(special[0]);
    if (i % 64 < n) return special[(i % 64 + seed) % n];
    return 0x3f000000u | (((static_cast<uint32_t>(i) * 0x9e3779b9u) ^ (seed * 0x85ebca6bu)) & 0x807fffffu);
}

struct Images {
    Words hist, norm, snap;
    Images(int slots, size_t t, uint32_t seed)
        : hist(hist_offset(slots) + kGuard, kSentinel), norm(norm_offset(t) + kGuard, kSentinel),
          snap(snap_offset(t + 1) + kGuard, kSentinel) {
        for (size_t i = kGuard; i < hist.size() - kGuard; ++i) hist[i] = pattern(i, seed);
        for (size_t i = kGuard; i < norm.size() - kGuard; ++i) norm[i] = pattern(i, seed + 5);
    }
};

struct DeviceImages {
    Buffer hist, norm, snap;
    explicit DeviceImages(const Images& image) : hist(image.hist.size()), norm(image.norm.size()), snap(image.snap.size()) {}
    void upload_window(const Images& image, hipStream_t stream) const {
        norm.upload(image.norm, stream);
        snap.upload(image.snap, stream);
    }
    void upload(const Images& image, hipStream_t stream) const {
        hist.upload(image.hist, stream);
        upload_window(image, stream);
    }
};

void expect_bits(const Words& expected, const Words& actual, const std::string& label) {
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == actual[i]) continue;
        char mismatch[128];
        std::snprintf(mismatch, sizeof(mismatch), " word %zu: expected 0x%08x, got 0x%08x", i,
                      static_cast<unsigned>(expected[i]), static_cast<unsigned>(actual[i]));
        throw std::runtime_error(label + mismatch);
    }
}

void check_images(const DeviceImages& old, const DeviceImages& fused, const Images& expected,
                  hipStream_t stream, const std::string& label) {
    HIP_CHECK(hipStreamSynchronize(stream));
    auto check = [&](const Buffer& ref, const Buffer& got, const Words& bits, const char* name) {
        const Words legacy = ref.read(bits.size());
        expect_bits(bits, legacy, label + " legacy " + name + " vs host");
        expect_bits(legacy, got.read(bits.size()), label + " fused " + name + " vs legacy");
    };
    check(old.hist, fused.hist, expected.hist, "history");
    check(old.norm, fused.norm, expected.norm, "normalized input");
    check(old.snap, fused.snap, expected.snap, "snapshots");
}

void advance_host(Images& image, const Rows& rows, bool snapshot) {
    for (size_t t = 0; t < rows.size(); ++t) {
        auto* hist = image.hist.data() + hist_offset(rows[t]);
        for (size_t c = 0; c < kChannels; ++c) {
            auto* column = hist + c * kDepth;
            std::memmove(column, column + 1, (kDepth - 1) * sizeof(uint32_t));
            column[kDepth - 1] = image.norm[norm_offset(t) + c];
        }
        if (snapshot) std::copy_n(hist, kHistory, image.snap.data() + snap_offset(t));
    }
}

void advance_device(const DeviceImages& image, const Rows& rows, bool fused, bool snapshot, hipStream_t stream) {
    for (size_t t = 0; t < rows.size(); ++t) {
        auto* hist = image.hist.floats() + hist_offset(rows[t]);
        auto* norm = image.norm.floats() + norm_offset(t);
        auto* snap = snapshot ? image.snap.floats() + snap_offset(t) : nullptr;
        if (fused) {
            k::ple_history_advance_snap(hist, norm, snap, stream);
        } else {
            k::ple_history_advance(hist, norm, stream);
            if (snapshot) k::copy_from_mapped(snap, hist, kHistory, stream);
        }
    }
}

void rollback_and_continue(const DeviceImages& old, const DeviceImages& fused, Images expected,
                           const Rows& rows, int slots, hipStream_t stream, const std::string& label) {
    // The verifier passes the start of each compact group and a device index of
    // accepted_count - 1. Keep one fewer token from each group that has a tail.
    Words indices(slots, 0);
    for (size_t first = 0, end = 0; first < rows.size(); first = end) {
        end = first + 1;
        const int slot = rows[first];
        while (end < rows.size() && rows[end] == slot) ++end;
        const size_t keep = std::max<size_t>(1, end - first - 1);
        indices[slot] = static_cast<uint32_t>(keep - 1);
        std::copy_n(expected.snap.data() + snap_offset(first + keep - 1), kHistory,
                    expected.hist.data() + hist_offset(slot));
    }
    Buffer d_indices(indices.size());
    d_indices.upload(indices, stream);
    for (size_t first = 0; first < rows.size(); ++first) {
        const int slot = rows[first];
        if (first != 0 && rows[first - 1] == slot) continue;
        const auto* index = static_cast<const int32_t*>(d_indices.ptr) + slot;
        for (const auto* image : {&old, &fused}) {
            k::copy_indexed(image->hist.floats() + hist_offset(slot), image->snap.floats() + snap_offset(first),
                            kHistory, index, kHistory, stream);
        }
    }
    check_images(old, fused, expected, stream, label + " restored prefix");

    // Only update the inputs and poison the snapshot outputs. The next window
    // must start from the actual device-restored state, not a fresh upload.
    const Images next(slots, rows.size(), 91);
    old.upload_window(next, stream);
    fused.upload_window(next, stream);
    expected.norm = next.norm;
    expected.snap = next.snap;
    advance_host(expected, rows, true);
    advance_device(old, rows, false, true, stream);
    advance_device(fused, rows, true, true, stream);
    check_images(old, fused, expected, stream, label + " following window");
}

struct Graph {
    hipGraph_t graph = nullptr;
    hipGraphExec_t exec = nullptr;
    ~Graph() {
        if (exec) (void) hipGraphExecDestroy(exec);
        if (graph) (void) hipGraphDestroy(graph);
    }
};

void test_window(const Rows& rows, int slots, hipStream_t stream, bool shorten) {
    const std::string label = (slots == 1 ? "single slot" : "compact batch") + std::string(" T=") +
                              std::to_string(rows.size());
    const Images initial(slots, rows.size(), 17);
    const DeviceImages old(initial), fused(initial);
    auto reset = [&](const Images& image) {
        old.upload(image, stream);
        fused.upload(image, stream);
    };

    reset(initial);
    Images expected = initial;
    advance_host(expected, rows, true);
    advance_device(old, rows, false, true, stream);
    advance_device(fused, rows, true, true, stream);
    check_images(old, fused, expected, stream, label + " eager");

    Graph graph;
    HIP_CHECK(hipStreamBeginCapture(stream, hipStreamCaptureModeThreadLocal));
    advance_device(fused, rows, true, true, stream);
    HIP_CHECK(hipStreamEndCapture(stream, &graph.graph));
    HIP_CHECK(hipGraphInstantiate(&graph.exec, graph.graph, nullptr, nullptr, 0));
    for (int replay = 0; replay < kReplays; ++replay) {
        const Images input(slots, rows.size(), 101u + 13u * replay);
        reset(input);
        advance_device(old, rows, false, true, stream);
        HIP_CHECK(hipGraphLaunch(graph.exec, stream));
        expected = input;
        advance_host(expected, rows, true);
        const std::string replay_label = label + " graph replay " + std::to_string(replay + 1);
        check_images(old, fused, expected, stream, replay_label);
        if (shorten && replay == kReplays - 1)
            rollback_and_continue(old, fused, expected, rows, slots, stream, replay_label);
    }

    reset(initial);
    expected = initial;
    advance_host(expected, rows, false);
    advance_device(old, rows, false, false, stream);
    advance_device(fused, rows, true, false, stream);
    check_images(old, fused, expected, stream, label + " null snapshot");
}

void test_invalid_arguments(hipStream_t stream) {
    Words initial(2 * kGuard + 4 * kHistory + kChannels, kSentinel);
    for (size_t i = kGuard; i < initial.size() - kGuard; ++i) initial[i] = pattern(i, 47);
    Buffer arena(initial.size());
    arena.upload(initial, stream);
    auto* hist = arena.floats() + kGuard;
    auto* norm = hist + 2 * kHistory;
    auto* snap = norm + kChannels + kHistory;
    auto reject = [stream](const char* name, float* h, const float* n, float* s) {
        try {
            k::ple_history_advance_snap(h, n, s, stream);
        } catch (const std::invalid_argument&) {
            return;
        }
        throw std::runtime_error(std::string("expected std::invalid_argument for ") + name);
    };
    // Four-word overlaps keep every argument aligned and each span in the allocation.
    reject("null history", nullptr, norm, snap);
    reject("null normalized input", hist, nullptr, snap);
    reject("history / normalized overlap", hist, hist + kHistory - 4, snap);
    reject("history / snapshot overlap", hist, norm, hist + kHistory - 4);
    reject("normalized / snapshot overlap", hist, norm, norm + kChannels - 4);
    reject("history / normalized overlap with no snapshot", hist, hist + kHistory - 4, nullptr);
    HIP_CHECK(hipStreamSynchronize(stream));
    expect_bits(initial, arena.read(initial.size()), "rejected argument buffers");
}
}  // namespace

int main(int argc, char** argv) {
    int device = 0;
    bool valid = argc <= 2;
    if (argc == 2) {
        const char* end = argv[1] + std::strlen(argv[1]);
        const auto parsed = std::from_chars(argv[1], end, device);
        valid = parsed.ec == std::errc{} && parsed.ptr == end && device >= 0;
    }
    if (!valid) {
        std::fprintf(stderr, "Usage: hip_ple_history_snap_test [device-id]\n");
        return 2;
    }

    try {
        int count = 0;
        const auto status = hipGetDeviceCount(&count);
        if (status == hipErrorNoDevice || (status == hipSuccess && count == 0)) {
            std::fprintf(stderr, "hip_ple_history_snap_test: no HIP devices available\n");
            return 1;
        }
        hip_check(status, "hipGetDeviceCount");
        if (device >= count) {
            std::fprintf(stderr, "hip_ple_history_snap_test: device %d out of range (found %d)\n", device, count);
            return 2;
        }
        HIP_CHECK(hipSetDevice(device));
        hipStream_t stream = nullptr;
        HIP_CHECK(hipStreamCreateWithFlags(&stream, hipStreamNonBlocking));
        test_invalid_arguments(stream);
        for (int t = 1; t <= 4; ++t) test_window(Rows(t, 0), 1, stream, t > 1);
        // Nonconsecutive slot IDs, including an untouched live slot, and no padding
        // between compact row groups. Exercise rollback with a nonzero group base.
        test_window({2, 0}, 3, stream, false);
        test_window({2, 0, 0}, 3, stream, true);
        test_window({2, 2, 0, 0}, 3, stream, true);
        test_window({2, 0, 0, 0}, 3, stream, true);  // Nonzero group base and restore index.
        HIP_CHECK(hipStreamDestroy(stream));
        std::printf("hip_ple_history_snap_test: PASS on device %d\n", device);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "hip_ple_history_snap_test: %s\n", e.what());
        return 1;
    }
}
