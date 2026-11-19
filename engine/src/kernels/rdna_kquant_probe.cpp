#include "strata/kernels/native_mmvq.hpp"
#include "strata/kernels/rdna_kquant_lab.hpp"
#include <cuda_runtime.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
namespace K = strata::kernels;
static void check(cudaError_t e) {
    if (e != cudaSuccess) { std::fprintf(stderr, "%s\n", cudaGetErrorString(e)); std::exit(2); }
}
template<class T> T* alloc(size_t n) { T* p; check(cudaMalloc((void**)&p, n*sizeof(T))); return p; }
int main(int argc, char** argv) {
    check(cudaSetDevice(argc > 1 ? std::atoi(argv[1]) : 0));
    cudaStream_t s; check(cudaStreamCreate(&s));
    std::mt19937 rng(191119); std::normal_distribution<float> normal;
    int bad = 0, cases = 0;
    for (int type : {12,13,14,21,23}) for (auto shape : std::vector<std::pair<int,int>>{
        {2560,640}, {2560,641}, {2560,10240}, {6144,2560}, {2560,512}}) {
        int ni=shape.first, no=shape.second;
        size_t block= type==12 ? 144 : type==13 ? 176 : type==14 ? 210 : type==21 ? 110 : 136;
        std::vector<unsigned char> w(K::native_mmvq_weight_bytes(type,ni,no));
        for (auto& v:w) v=static_cast<unsigned char>(rng());
        for (size_t i=0;i<w.size();i+=block) {
            uint16_t scale=uint16_t(0x2000+rng()%64);
            std::memcpy(w.data()+i+(type==14 ? 208 : 0),&scale,2);
            if(type==12 || type==13) std::memcpy(w.data()+i+2,&scale,2);
        }
        std::vector<float> x(size_t(ni)*16);
        for(auto& v:x) v=normal(rng);
        std::fill(x.end()-ni,x.end(),0.f);
        auto dw=alloc<unsigned char>(w.size()), dq=alloc<unsigned char>(K::native_q8_1_bytes(ni,16));
        auto dx=alloc<float>(x.size()), dy=alloc<float>(size_t(no)*16), dz=alloc<float>(size_t(no)*16);
        check(cudaMemcpy(dw,w.data(),w.size(),cudaMemcpyHostToDevice));
        check(cudaMemcpy(dx,x.data(),x.size()*4,cudaMemcpyHostToDevice));
        K::native_quantize_q8_1(dx,dq,ni,16,s); check(cudaStreamSynchronize(s));
        for(int nc=1;nc<=16;++nc) {
            std::vector<float> out[2];
            for(int alt=0;alt<2;++alt) {
                K::native_mmvq_rdna_kquant_probe_mode(alt);
                K::native_mmvq(type,dw,dq,dy,ni,no,nc,s);
                check(cudaGetLastError()); check(cudaStreamSynchronize(s));
                out[alt].resize(size_t(no)*nc);
                check(cudaMemcpy(out[alt].data(),dy,out[alt].size()*4,cudaMemcpyDeviceToHost));
            }
            double diff2=0,ref2=0,ma=0,mr=0; bool finite=true;
            for(size_t i=0;i<out[0].size();++i) {
                double a=out[0][i],b=out[1][i]; finite=finite&&std::isfinite(b);
                diff2+=(a-b)*(a-b); ref2+=a*a; ma=std::max(ma,std::abs(a-b)); mr=std::max(mr,std::abs(a));
            }
            double rel=std::sqrt(diff2/std::max(ref2,1e-30));
            bool near=finite&&rel<=2e-5&&ma<=1e-5+2e-5*mr;
            // Each optimized column must match an independent optimized single-column call.
            bool column=true;
            if(nc<=8 && (type!=12 && type!=13)) {
                for(int c=0;c<nc;++c) K::native_mmvq(type,dw,dq+size_t(c)*K::native_q8_1_bytes(ni,1),dz+size_t(c)*no,ni,no,1,s);
                check(cudaStreamSynchronize(s)); std::vector<float> v(out[1].size());
                check(cudaMemcpy(v.data(),dz,v.size()*4,cudaMemcpyDeviceToHost));
                column=std::memcmp(v.data(),out[1].data(),v.size()*4)==0;
            }
            bool pair=true;
            if(nc<=8) {
                bool used=K::native_mmvq_pair(type,dw,dw,dq,dy,dz,ni,no,nc,s);
                if(used) {
                    check(cudaStreamSynchronize(s)); std::vector<float> a(out[1].size()),b(a.size());
                    check(cudaMemcpy(a.data(),dy,a.size()*4,cudaMemcpyDeviceToHost));
                    check(cudaMemcpy(b.data(),dz,b.size()*4,cudaMemcpyDeviceToHost));
                    pair=std::memcmp(a.data(),out[1].data(),a.size()*4)==0 && std::memcmp(b.data(),out[1].data(),b.size()*4)==0;
                }
            }
            bool zero=true; if(nc==16) for(int i=15*no;i<16*no;++i) zero=zero&&out[1][i]==0.f;
            bool ok=near&&column&&pair&&zero; ++cases; bad+=!ok;
            std::printf("type=%d in=%d out=%d cols=%d rel_l2=%.3g max_abs=%.3g column=%d pair=%d zero=%d passed=%d\n",type,ni,no,nc,rel,ma,column,pair,zero,ok);
            if(no==10240 && (nc==1 || nc==4)) {
                for(int alt=0;alt<2;++alt) {
                    K::native_mmvq_rdna_kquant_probe_mode(alt); cudaEvent_t a,b;
                    check(cudaEventCreate(&a)); check(cudaEventCreate(&b));
                    for(int i=0;i<10;++i) K::native_mmvq(type,dw,dq,dy,ni,no,nc,s);
                    check(cudaEventRecord(a,s));
                    for(int i=0;i<100;++i) K::native_mmvq(type,dw,dq,dy,ni,no,nc,s);
                    check(cudaEventRecord(b,s)); check(cudaEventSynchronize(b)); float ms;
                    check(cudaEventElapsedTime(&ms,a,b)); std::printf("BENCH type=%d cols=%d alt=%d us=%.4f\n",type,nc,alt,ms*10);
                    check(cudaEventDestroy(a)); check(cudaEventDestroy(b));
                }
                K::native_mmvq_rdna_kquant_probe_mode(1); cudaGraph_t g; cudaGraphExec_t exec;
                check(cudaStreamBeginCapture(s,cudaStreamCaptureModeThreadLocal)); K::native_mmvq(type,dw,dq,dy,ni,no,nc,s);
                check(cudaStreamEndCapture(s,&g)); check(cudaGraphInstantiate(&exec,g,nullptr,nullptr,0));
                for(int i=0;i<3;++i) check(cudaGraphLaunch(exec,s)); check(cudaStreamSynchronize(s));
                std::vector<float> v(out[1].size()); check(cudaMemcpy(v.data(),dy,v.size()*4,cudaMemcpyDeviceToHost));
                bool graph=std::memcmp(v.data(),out[1].data(),v.size()*4)==0; bad+=!graph;
                std::printf("GRAPH type=%d cols=%d bitwise=%d\n",type,nc,graph);
                check(cudaGraphExecDestroy(exec)); check(cudaGraphDestroy(g));
            }
        }
        check(cudaFree(dw));check(cudaFree(dq));check(cudaFree(dx));check(cudaFree(dy));check(cudaFree(dz));
    }
    check(cudaStreamDestroy(s)); std::printf("SUMMARY cases=%d bad=%d\n",cases,bad); return bad?1:0;
}
