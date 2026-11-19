#include "strata/kernels/native_mmvq.hpp"
#include <cuda_runtime.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
namespace K = strata::kernels;
namespace strata::kernels { void native_mmvq_rdna_probe_mode(int); }
static void check(cudaError_t e) {
    if (e != cudaSuccess) { std::fprintf(stderr, "%s\n", cudaGetErrorString(e)); std::exit(2); }
}
template<class T> T* allocate(size_t n) { T* p; check(cudaMalloc((void**)&p, n * sizeof(T))); return p; }
int main(int argc, char** argv) {
    check(cudaSetDevice(argc > 1 ? std::atoi(argv[1]) : 0));
    cudaStream_t stream; check(cudaStreamCreate(&stream)); int bad = 0, cases = 0;
    std::mt19937 rng(1911); std::normal_distribution<float> normal(0, 1);
    for (int type : {2, 6, 8, 20, 13}) for (auto shape : (type == 13
        ? std::vector<std::pair<int,int>>{{2560,65537}, {2560,248320}, {2560,6144}, {6144,1024}, {1024,256}}
        : std::vector<std::pair<int,int>>{{2048,1024}, {2560,2049}, {2560,6144}, {6144,1024}, {1024,256}})) {
        const int ni = shape.first, no = shape.second, block = type == 13 ? 176 : type == 8 ? 34 : type == 6 ? 22 : 18;
        const int div = type == 13 ? 256 : 32;
        std::vector<uint8_t> weights(size_t(ni / div) * no * block);
        for (auto& v : weights) v = uint8_t(rng());
        for (size_t i = 0; i < weights.size(); i += block) { uint16_t scale = uint16_t(0x2000 + rng() % 64); std::memcpy(weights.data() + i, &scale, 2); if (type == 13) std::memcpy(weights.data() + i + 2, &scale, 2); }
        std::vector<float> x(size_t(ni) * 16);
        for (auto& v : x) v = normal(rng);
        std::fill(x.end() - ni, x.end(), 0.f);
        auto dw = allocate<uint8_t>(weights.size()), dq = allocate<uint8_t>(K::native_q8_1_bytes(ni, 16));
        auto dx = allocate<float>(x.size()), dy = allocate<float>(size_t(no) * 16);
        check(cudaMemcpy(dw, weights.data(), weights.size(), cudaMemcpyHostToDevice));
        check(cudaMemcpy(dx, x.data(), x.size() * 4, cudaMemcpyHostToDevice));
        K::native_quantize_q8_1(dx, dq, ni, 16, stream); check(cudaStreamSynchronize(stream));
        for (int nc = 1; nc <= 16; ++nc) {
            std::vector<float> out[2];
            for (int alt = 0; alt < 2; ++alt) {
                K::native_mmvq_rdna_probe_mode(alt);
                K::native_mmvq(type, dw, dq, dy, ni, no, nc, stream);
                check(cudaGetLastError()); check(cudaStreamSynchronize(stream));
                out[alt].resize(size_t(no) * nc);
                check(cudaMemcpy(out[alt].data(), dy, out[alt].size() * 4, cudaMemcpyDeviceToHost));
            }
            const bool finite = std::all_of(out[1].begin(), out[1].end(), [](float v) { return std::isfinite(v); });
            const bool same = std::memcmp(out[0].data(), out[1].data(), out[0].size() * 4) == 0;
            bool zero = true; if (nc == 16) for (int i = 15 * no; i < 16 * no; ++i) zero = zero && out[1][i] == 0.f;
            const bool ok = finite && same && zero; bad += !ok; ++cases;
            std::printf("type=%d in=%d out=%d cols=%d bitwise=%d finite=%d zero=%d passed=%d\n", type, ni, no, nc, same, finite, zero, ok);
            if (ni == 2560 && ((type != 13 && no == 6144) || (type == 13 && no == 248320)) && (nc == 1 || nc == 4)) {
                for (int alt = 0; alt < 2; ++alt) {
                    K::native_mmvq_rdna_probe_mode(alt); cudaEvent_t a, b; check(cudaEventCreate(&a)); check(cudaEventCreate(&b));
                    for (int i = 0; i < 10; ++i) K::native_mmvq(type, dw, dq, dy, ni, no, nc, stream);
                    check(cudaEventRecord(a, stream));
                    for (int i = 0; i < 100; ++i) K::native_mmvq(type, dw, dq, dy, ni, no, nc, stream);
                    check(cudaEventRecord(b, stream)); check(cudaEventSynchronize(b)); float ms; check(cudaEventElapsedTime(&ms, a, b));
                    std::printf("BENCH type=%d cols=%d alt=%d us=%.4f\n", type, nc, alt, ms * 10); check(cudaEventDestroy(a)); check(cudaEventDestroy(b));
                }
                K::native_mmvq_rdna_probe_mode(1); cudaGraph_t g; cudaGraphExec_t exec;
                check(cudaStreamBeginCapture(stream, cudaStreamCaptureModeThreadLocal)); K::native_mmvq(type, dw, dq, dy, ni, no, nc, stream);
                check(cudaStreamEndCapture(stream, &g)); check(cudaGraphInstantiate(&exec, g, nullptr, nullptr, 0));
                for (int i = 0; i < 3; ++i) check(cudaGraphLaunch(exec, stream));
                check(cudaStreamSynchronize(stream));
                std::vector<float> actual(size_t(no) * nc); check(cudaMemcpy(actual.data(), dy, actual.size() * 4, cudaMemcpyDeviceToHost));
                const bool graph_ok = std::memcmp(actual.data(), out[0].data(), actual.size() * 4) == 0;
                bad += !graph_ok; std::printf("GRAPH type=%d cols=%d bitwise=%d\n", type, nc, graph_ok);
                check(cudaGraphExecDestroy(exec)); check(cudaGraphDestroy(g));
            }
        }
        check(cudaFree(dw)); check(cudaFree(dq)); check(cudaFree(dx)); check(cudaFree(dy));
    }
    std::printf("SUMMARY cases=%d bad=%d\n", cases, bad); return bad ? 1 : 0;
}
