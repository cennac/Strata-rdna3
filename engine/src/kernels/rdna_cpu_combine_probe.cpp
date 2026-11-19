#include "strata/kernels/native_moe.hpp"
#include "strata/kernels/elementwise.hpp"
#include "strata/kernels/s2_expert_grouped.hpp"
#include "strata/kernels/verify_kernels.hpp"
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>
namespace K = strata::kernels;
static void ck(cudaError_t e) { if (e != cudaSuccess) throw std::runtime_error(cudaGetErrorString(e)); }
template<class T> static T* alloc(size_t n) { T* p; ck(cudaMalloc((void**)&p,n*sizeof(T))); return p; }
int main(int argc, char** argv) {
 try {
    ck(cudaSetDevice(argc>1?std::atoi(argv[1]):0));
    cudaStream_t stream; ck(cudaStreamCreate(&stream));
    int bad=0, checks=0, invalid=0;
    std::mt19937 rng(261119); std::uniform_real_distribution<float> dist(-2.f,2.f);
    for (int N : {4, 12, 252, 2560, 2564}) for (int T=1;T<=8;++T) {
        const size_t rows=T*10, count=rows*N, outsize=size_t(T)*N;
        float *host,*mapped; ck(cudaHostAlloc((void**)&host,count*4,cudaHostAllocMapped));
        ck(cudaHostGetDevicePointer((void**)&mapped,host,0));
        uint32_t *hf,*df; ck(cudaHostAlloc((void**)&hf,4,cudaHostAllocMapped));
        ck(cudaHostGetDevicePointer((void**)&df,hf,0));
        auto hit=alloc<float>(count), part=alloc<float>(count), w=alloc<float>(rows), sh=alloc<float>(outsize);
        auto dst=alloc<int32_t>(rows), nh=alloc<int32_t>(1); auto a=alloc<float>(outsize+8), b=alloc<float>(outsize+8);
        std::vector<float> hhit(count), hw(rows), hsh(outsize), ref(outsize+8), got(outsize+8);
        for(auto&v:hhit)v=dist(rng); for(auto&v:hw)v=dist(rng); for(auto&v:hsh)v=dist(rng);
        // Signed inputs, signed zero and a wide dynamic range. Replay 2 below
        // checks fully zero rows so zero signs cannot hide behind other terms.
        for(size_t i=0;i<count;i+=31)hhit[i]=-0.f;
        for(size_t i=7;i<count;i+=61)hhit[i]*=1e10f;
        ck(cudaMemcpy(hit,hhit.data(),count*4,cudaMemcpyHostToDevice));
        ck(cudaMemcpy(w,hw.data(),rows*4,cudaMemcpyHostToDevice));
        ck(cudaMemcpy(sh,hsh.data(),outsize*4,cudaMemcpyHostToDevice));
        for(int mask=0;mask<5;++mask) {
            std::vector<int32_t> hd;
            for(size_t r=0;r<rows;++r)if(mask==1 || (mask==2&&r%2) || (mask==3&&r%10==0) || (mask==4&&rng()%3==0))hd.push_back(int(r));
            int hn=int(hd.size());
            ck(cudaMemcpy(nh,&hn,4,cudaMemcpyHostToDevice));
            if(hn)ck(cudaMemcpy(dst,hd.data(),hn*4,cudaMemcpyHostToDevice));
            auto reference=[&] {
                K::wait_flag_ge(df,1,stream);
                K::copy_rows_from_mapped(part,mapped,rows,N,dst,nh,stream);
                K::moe_hit_add(part,hit,dst,nh,rows,N,stream);
                K::native_moe_combine_multi(part,w,sh,a+4,N,10,T,stream);
            };
            auto candidate=[&] {
                K::wait_flag_ge(df,1,stream);
                K::native_moe_combine_multi_cpu(mapped,hit,dst,nh,w,sh,b+4,N,T,stream);
            };
            // Capture while the publication flag is still zero. Replay only
            // after a delayed CPU write, then reuse the same rows and graph.
            *hf=0;
            cudaGraph_t graph; cudaGraphExec_t exec;
            ck(cudaStreamBeginCapture(stream,cudaStreamCaptureModeThreadLocal));
            reference();candidate();
            ck(cudaStreamEndCapture(stream,&graph));
            ck(cudaGraphInstantiate(&exec,graph,nullptr,nullptr,0));
            for(int replay=0;replay<3;++replay) {
                ck(cudaMemset(a,0xa5,(outsize+8)*4)); ck(cudaMemset(b,0xa5,(outsize+8)*4));
                *hf=0;
                std::vector<float> next(count);for(auto&v:next)v=replay==2?-0.f:dist(rng);
                std::vector<float> zero_hit(count,-0.f),zero_shared(outsize,-0.f);
                ck(cudaMemcpy(hit,replay==2?zero_hit.data():hhit.data(),count*4,cudaMemcpyHostToDevice));
                ck(cudaMemcpy(sh,replay==2?zero_shared.data():hsh.data(),outsize*4,cudaMemcpyHostToDevice));
                std::thread publish([&] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    std::memcpy(host,next.data(),count*4);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    __atomic_store_n(hf,1u,__ATOMIC_RELEASE);
                });
                ck(cudaGraphLaunch(exec,stream));
                auto sync=cudaStreamSynchronize(stream);publish.join();ck(sync);
                ck(cudaMemcpy(ref.data(),a,(outsize+8)*4,cudaMemcpyDeviceToHost));
                ck(cudaMemcpy(got.data(),b,(outsize+8)*4,cudaMemcpyDeviceToHost));
                bool equal=std::memcmp(ref.data(),got.data(),(outsize+8)*4)==0;
                uint32_t canary=0xa5a5a5a5;bool guard=true;
                for(size_t i: {size_t(0),size_t(1),size_t(2),size_t(3),outsize+4,outsize+5,outsize+6,outsize+7})
                    guard &= std::memcmp(&got[i],&canary,4)==0 && std::memcmp(&ref[i],&canary,4)==0;
                ++checks;if(!equal||!guard)++bad;
                std::printf("CPU_COMBINE N=%d T=%d mask=%d replay=%d bitwise=%d canary=%d\n",N,T,mask,replay,int(equal),int(guard));
            }
            ck(cudaGraphExecDestroy(exec));ck(cudaGraphDestroy(graph));
        }
        auto reject=[&](auto f){++invalid;try{f();++bad;}catch(const std::invalid_argument&){};};
        reject([&]{K::native_moe_combine_multi_cpu(nullptr,hit,dst,nh,w,sh,b+4,N,T,stream);});
        reject([&]{K::native_moe_combine_multi_cpu(mapped,hit,dst,nh,w,sh,b+4,N,9,stream);});
        reject([&]{K::native_moe_combine_multi_cpu(mapped,hit,dst,nh,w,sh,b+4,N-1,T,stream);});
        reject([&]{K::native_moe_combine_multi_cpu(mapped,hit,dst,nh,w,sh,hit,N,T,stream);});
        reject([&]{K::native_moe_combine_multi_cpu(mapped,hit,dst,nh,w,sh,b+1,N,T,stream);});
        reject([&]{K::native_moe_combine_multi_cpu(mapped,hit,dst,nullptr,w,sh,b+4,N,T,stream);});
        for(void*p:{(void*)hit,(void*)part,(void*)w,(void*)sh,(void*)dst,(void*)nh,(void*)a,(void*)b})ck(cudaFree(p));
        ck(cudaFreeHost(host));ck(cudaFreeHost(hf));
    }
    ck(cudaStreamDestroy(stream));
    std::printf("SUMMARY checks=%d invalid=%d bad=%d\n",checks,invalid,bad);
    return bad?1:0;
 } catch(const std::exception&e) {std::fprintf(stderr,"ERROR %s\n",e.what());return 2;}
}
