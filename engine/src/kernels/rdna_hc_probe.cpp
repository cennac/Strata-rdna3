#include "strata/kernels/fused_gr.hpp"
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace K=strata::kernels;
namespace strata::kernels { void rdna_gr_read_multi(const FusedGrArgs*,int,float*,void*,unsigned long long*,int); }
static void ck(cudaError_t e){if(e!=cudaSuccess){std::fprintf(stderr,"%s\n",cudaGetErrorString(e));std::exit(2);}}
template<class T> T* alloc(size_t n){T*p=nullptr;ck(cudaMalloc((void**)&p,n*sizeof(T)));return p;}
template<class T> void put(T*p,const std::vector<T>&v){ck(cudaMemcpy(p,v.data(),v.size()*sizeof(T),cudaMemcpyHostToDevice));}
static uint16_t bf(float f){uint32_t u;std::memcpy(&u,&f,4);return (u+0x7fff+((u>>16)&1))>>16;}
static std::vector<float> get(float*p,size_t n){std::vector<float>v(n);ck(cudaMemcpy(v.data(),p,n*4,cudaMemcpyDeviceToHost));return v;}
int main(int argc,char**argv){
 ck(cudaSetDevice(argc>1?std::atoi(argv[1]):0));setenv("STRATA_LAB_RDNA_GR","0",1);
 constexpr int N=2560,H=4,D=N*H,L=320,TM=8;
 std::mt19937 rng(1719);std::normal_distribution<float> nd(0,1);
 std::vector<float> R(TM*D),bo(TM*N),inj(TM*H),wn(D);
 for(auto&x:R)x=nd(rng);for(auto&x:bo)x=.5f*nd(rng);for(auto&x:inj)x=nd(rng);for(auto&x:wn)x=1+.3f*nd(rng);
 std::vector<uint16_t> wd(L*D),wu(D*L),wi(H*D);
 for(auto&x:wd)x=bf(.02f*nd(rng));for(auto&x:wu)x=bf(.05f*nd(rng));for(auto&x:wi)x=bf(.02f*nd(rng));
 auto r=alloc<float>(R.size()),b=alloc<float>(bo.size()),j=alloc<float>(inj.size()),w=alloc<float>(D);
 auto down=alloc<uint16_t>(wd.size()),up=alloc<uint16_t>(wu.size()),inject=alloc<uint16_t>(wi.size());
 auto scratch=alloc<float>(TM*D),lo=alloc<float>(TM*L),rs=alloc<float>(TM*H),io=alloc<float>(TM*H),mix=alloc<float>(TM*N);
 put(b,bo);put(j,inj);put(w,wn);put(down,wd);put(up,wu);put(inject,wi);
 cudaStream_t stream;ck(cudaStreamCreate(&stream));int bad=0;
 const char*names[]={"R","lo","rs","inject","mixed"};
 for(int t=1;t<=8;++t)for(int apply=0;apply<2;++apply)for(int in=0;in<2;++in){
  K::FusedGrArgs a[TM];for(int k=0;k<t;++k){auto&x=a[k];x.R=r+k*D;x.R_out=r+k*D;x.apply=apply;x.bo_prev=b+k*N;x.inj_prev=j+k*H;x.w_norm=w;x.w_down=down;x.w_up=up;x.w_inject=in?inject:nullptr;x.lo=lo+k*L;x.rs=rs+k*H;x.inject_out=io+k*H;x.mixed=mix+k*N;}
  std::vector<float> out[2][5];
  for(int alt=0;alt<2;++alt){
   put(r,R);ck(cudaMemset(io,0,TM*H*4));
   if(alt)K::rdna_gr_read_multi(a,t,scratch,stream,nullptr,0);else K::fused_gr_read_multi(a,t,scratch,stream);
   ck(cudaGetLastError());ck(cudaStreamSynchronize(stream));
   out[alt][0]=get(r,t*D);out[alt][1]=get(lo,t*L);out[alt][2]=get(rs,t*H);out[alt][3]=get(io,t*H);out[alt][4]=get(mix,t*N);
  }
  for(int f=0;f<5;++f){double ss=0,dd=0,mx=0;bool finite=true;
   for(size_t k=0;k<out[0][f].size();++k){double x=out[0][f][k],y=out[1][f][k];finite=finite&&std::isfinite(x)&&std::isfinite(y);ss+=x*x;dd+=(x-y)*(x-y);mx=std::fmax(mx,std::fabs(x-y));}
   double rel=std::sqrt(dd/std::fmax(ss,1e-30));bool ok=finite&&rel<1e-4;bad+=!ok;
   std::printf("T=%d apply=%d inject=%d field=%s relative_l2=%.9g max_abs=%.9g finite=%d passed=%d\n",t,apply,in,names[f],rel,mx,finite,ok);
  }
  if(!apply&&in&&(t==1||t==4))for(int alt=0;alt<2;++alt){
   auto run=[&]{if(alt)K::rdna_gr_read_multi(a,t,scratch,stream,nullptr,0);else K::fused_gr_read_multi(a,t,scratch,stream);};
   for(int z=0;z<20;++z)run();ck(cudaStreamSynchronize(stream));cudaEvent_t e0,e1;ck(cudaEventCreate(&e0));ck(cudaEventCreate(&e1));ck(cudaEventRecord(e0,stream));for(int z=0;z<200;++z)run();ck(cudaEventRecord(e1,stream));ck(cudaEventSynchronize(e1));float ms;ck(cudaEventElapsedTime(&ms,e0,e1));std::printf("BENCH T=%d alt=%d microseconds=%.4f\n",t,alt,ms*1000/200);ck(cudaEventDestroy(e0));ck(cudaEventDestroy(e1));
  }
 }
 std::printf("SUMMARY bad=%d\n",bad);return bad?1:0;
}
