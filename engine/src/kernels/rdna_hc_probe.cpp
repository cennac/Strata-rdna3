#include "strata/kernels/fused_gr.hpp"
#include "strata/kernels/native_mmvq.hpp"
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
namespace K=strata::kernels;
namespace strata::kernels { void rdna_gr_read_multi(const FusedGrArgs*,int,float*,void*,unsigned long long*,int); }
namespace strata::kernels { void rdna_gr_q32_probe_mode(int); }
static void ck(cudaError_t e){if(e!=cudaSuccess){std::fprintf(stderr,"%s\n",cudaGetErrorString(e));std::exit(2);}}
template<class T> T* alloc(size_t n){T*p=nullptr;ck(cudaMalloc((void**)&p,n*sizeof(T)));return p;}
template<class T> void put(T*p,const std::vector<T>&v){ck(cudaMemcpy(p,v.data(),v.size()*sizeof(T),cudaMemcpyHostToDevice));}
static uint16_t bf(float f){uint32_t u;std::memcpy(&u,&f,4);return (u+0x7fff+((u>>16)&1))>>16;}
static std::vector<float> get(float*p,size_t n){std::vector<float>v(n);ck(cudaMemcpy(v.data(),p,n*4,cudaMemcpyDeviceToHost));return v;}
static float from_bf(uint16_t x){uint32_t u=uint32_t(x)<<16;float f;std::memcpy(&f,&u,4);return f;}
static float from_half(uint16_t x){
 unsigned e=(x>>10)&31,m=x&1023;float f=e?std::ldexp(1.f+float(m)/1024,int(e)-15):std::ldexp(float(m),-24);
 return (x&32768)?-f:f;
}
struct Q8Host {
 std::vector<int8_t> q;std::vector<float> d;
 explicit Q8Host(const std::vector<uint8_t>& b):q(b.size()/34*32),d(b.size()/34){
  for(size_t k=0;k<d.size();++k){uint16_t h;std::memcpy(&h,b.data()+k*34,2);d[k]=from_half(h);std::memcpy(q.data()+32*k,b.data()+34*k+2,32);}
 }
};
static void activation_q8(const std::vector<float>& x,std::vector<int8_t>& q,std::vector<float>& d){
 q.resize(x.size());d.resize(x.size()/32);
 for(size_t k=0;k<d.size();++k){float a=0;for(int i=0;i<32;++i)a=std::max(a,std::fabs(x[k*32+i]));
  d[k]=a/127;float inv=a?127/a:0;for(int i=0;i<32;++i)q[k*32+i]=int8_t(std::nearbyint(x[k*32+i]*inv));}
}
// Independent scalar reference for the explicitly approximate A8 path. Weights are the actual
// upstream Q8_0 bytes; activations use per-32 round-to-nearest-even, not the BF16 reference.
static std::vector<std::vector<float>> a8_reference(const std::vector<float>& R,const std::vector<float>& bo,
 const std::vector<float>& inj,const std::vector<float>& wn,const std::vector<uint16_t>& wi,
 const Q8Host& down,const Q8Host& up,int apply,int inject){
 constexpr int N=2560,H=4,D=N*H,L=320,TM=8;
 std::vector<std::vector<float>> out={R,std::vector<float>(TM*L),std::vector<float>(TM*H),std::vector<float>(TM*H),std::vector<float>(TM*N)};
 auto sig=[](float x){return 1.f/(1.f+std::exp(-x));};
 for(int k=0;k<TM;++k){std::vector<float>x(D),lo(L);float rs[H];
  for(int c=0;c<H;++c){double ss=0;float gate=2*sig(inj[k*H+c]/H);
   for(int i=0;i<N;++i){int e=k*D+c*N+i;float r=R[e];if(apply)r=std::fma(bo[k*N+i],gate,r);out[0][e]=r;ss+=double(r)*r;x[c*N+i]=r*wn[c*N+i];}
   rs[c]=1/std::sqrt(float(ss/N)+1e-6f);out[2][k*H+c]=rs[c];}
  std::vector<int8_t>xq,lq;std::vector<float>xd,ld;activation_q8(x,xq,xd);
  for(int row=0;row<L;++row){double sum=0;
   for(int b=0;b<D/32;++b){int dot=0;for(int j=0;j<32;++j)dot+=int(down.q[row*D+b*32+j])*int(xq[b*32+j]);
    sum+=double(dot)*(down.d[row*(D/32)+b]*xd[b])*rs[(b*32)/N];}
   float v=float(sum/H);lo[row]=v*sig(v);out[1][k*L+row]=lo[row];}
  if(inject)for(int row=0;row<H;++row){double sum=0;for(int e=0;e<D;++e)sum+=double(from_bf(wi[row*D+e]))*x[e]*rs[e/N];out[3][k*H+row]=float(sum);}
  activation_q8(lo,lq,ld);
  for(int i=0;i<N;++i){float mixed=0;for(int c=0;c<H;++c){int row=c*N+i;double sum=0;
    for(int b=0;b<L/32;++b){int dot=0;for(int j=0;j<32;++j)dot+=int(up.q[row*L+b*32+j])*int(lq[b*32+j]);sum+=double(dot)*(up.d[row*(L/32)+b]*ld[b]);}
    mixed+=x[row]*rs[c]*sig(float(sum));}out[4][k*N+i]=mixed/H;}
 }
 return out;
}
int main(int argc,char**argv){
 ck(cudaSetDevice(argc>1?std::atoi(argv[1]):0));setenv("STRATA_LAB_RDNA_GR","0",1);
 const bool q32=argc>2&&(std::strcmp(argv[2],"q32")==0||std::strcmp(argv[2],"q32plain")==0);
 const bool q32_image=q32&&std::strcmp(argv[2],"q32")==0;
 const char* policy=std::getenv("STRATA_QFUSE_HC_LOCAL");
 const bool q32_policy_off=q32&&policy&&policy[0]=='0';
 if(q32)setenv("STRATA_LAB_RDNA_GR","1",1);
 const bool q8=!q32&&argc>2&&std::strcmp(argv[2],"bf16")!=0,a8=q8&&std::strcmp(argv[2],"a8")==0;
 setenv("STRATA_GR_RDNA_A8",a8?"1":"0",1);
 constexpr int N=2560,H=4,D=N*H,L=320,TM=8;
 std::mt19937 rng(1719);std::normal_distribution<float> nd(0,1);
 std::vector<float> R(TM*D),bo(TM*N),inj(TM*H),wn(D);
 for(auto&x:R)x=nd(rng);for(auto&x:bo)x=.5f*nd(rng);for(auto&x:inj)x=nd(rng);for(auto&x:wn)x=1+.3f*nd(rng);
 if(std::getenv("STRATA_RDNA_PROBE_ZERO")){std::fill(R.begin(),R.end(),0.f);std::fill(bo.begin(),bo.end(),0.f);std::fill(inj.begin(),inj.end(),0.f);}
 std::vector<uint16_t> wd(L*D),wu(D*L),wi(H*D);
 for(auto&x:wd)x=bf(.02f*nd(rng));for(auto&x:wu)x=bf(.05f*nd(rng));for(auto&x:wi)x=bf(.02f*nd(rng));
 auto r=alloc<float>(R.size()),b=alloc<float>(bo.size()),j=alloc<float>(inj.size()),w=alloc<float>(D);
 auto down=alloc<uint16_t>(wd.size()),up=alloc<uint16_t>(wu.size()),inject=alloc<uint16_t>(wi.size());
 auto scratch=alloc<float>(TM*D),lo=alloc<float>(TM*L),rs=alloc<float>(TM*H),io=alloc<float>(TM*H),mix=alloc<float>(TM*N);
 put(b,bo);put(j,inj);put(w,wn);put(down,wd);put(up,wu);put(inject,wi);
 cudaStream_t stream;ck(cudaStreamCreate(&stream));int bad=0;
 const size_t qstride=K::native_q8_1_bytes(N),qbytes=qstride*TM;
 auto qm=alloc<uint8_t>(qbytes+64),qr=alloc<uint8_t>(qbytes);
 std::vector<uint8_t> qreference;
 uint8_t *qd=nullptr,*qu=nullptr;std::vector<uint8_t> hd,hu;
 if(q8){qd=alloc<uint8_t>(wd.size()/32*34);qu=alloc<uint8_t>(wu.size()/32*34);
  K::fused_gr_quantize_q8_0(down,qd,wd.size()/32,stream);K::fused_gr_quantize_q8_0(up,qu,wu.size()/32,stream);ck(cudaStreamSynchronize(stream));
  hd.resize(wd.size()/32*34);hu.resize(wu.size()/32*34);ck(cudaMemcpy(hd.data(),qd,hd.size(),cudaMemcpyDeviceToHost));ck(cudaMemcpy(hu.data(),qu,hu.size(),cudaMemcpyDeviceToHost));}
 std::vector<std::vector<float>> golden[2][2];
 if(a8)for(int apply=0;apply<2;++apply)for(int in=0;in<2;++in)golden[apply][in]=a8_reference(R,bo,inj,wn,wi,Q8Host(hd),Q8Host(hu),apply,in);
 std::printf("MODE %s reference=%s\n",q32?(q32_image?"q32":"q32plain"):a8?"a8":q8?"q8":"bf16",q32?"legacy RDNA16 bitwise + separate native q8_1":a8?"independent scalar Q8-weight/A8-activation":"upstream GPU read");
 const char*names[]={"R","lo","rs","inject","mixed"};
 for(int t=1;t<=8;++t)for(int apply=0;apply<2;++apply)for(int in=0;in<2;++in){
  K::FusedGrArgs a[TM];for(int k=0;k<t;++k){auto&x=a[k];x.R=r+k*D;x.R_out=r+k*D;x.apply=apply;x.bo_prev=b+k*N;x.inj_prev=j+k*H;x.w_norm=w;x.w_down=down;x.w_up=up;x.w_inject=in?inject:nullptr;x.lo=lo+k*L;x.rs=rs+k*H;x.inject_out=io+k*H;x.mixed=mix+k*N;x.q8_down=qd;x.q8_up=qu;}
  std::vector<float> out[2][5];
  for(int alt=0;alt<2;++alt){
   if(q32){K::rdna_gr_q32_probe_mode(alt);for(int k=0;k<t;++k)a[k].q8_mixed=alt&&q32_image?qm+k*qstride:nullptr;ck(cudaMemset(qm,0xa5,qbytes+64));}
   put(r,R);ck(cudaMemset(io,0,TM*H*4));
   if(!alt&&a8){const size_t sizes[]={size_t(t*D),size_t(t*L),size_t(t*H),size_t(t*H),size_t(t*N)};
    for(int f=0;f<5;++f)out[0][f]=std::vector<float>(golden[apply][in][f].begin(),golden[apply][in][f].begin()+sizes[f]);continue;}
   if(q32&&alt){
    const bool produced=K::fused_gr_read_multi(a,t,scratch,stream);
    const bool expected=q32_image&&!q32_policy_off;
    bad+=produced!=expected;
    std::printf("DISPATCH T=%d apply=%d inject=%d produced=%d expected=%d passed=%d\n",t,apply,in,produced,expected,produced==expected);
    if(q32_image&&q32_policy_off){
     ck(cudaStreamSynchronize(stream));std::vector<uint8_t> untouched(qbytes+64);ck(cudaMemcpy(untouched.data(),qm,untouched.size(),cudaMemcpyDeviceToHost));
     const bool clean=std::all_of(untouched.begin(),untouched.end(),[](uint8_t v){return v==0xa5;});bad+=!clean;
     std::printf("FALLBACK T=%d image_untouched=%d\n",t,clean);
     K::native_quantize_q8_1(mix,qm,N,t,stream);
    }
   }else if(alt||q32)K::rdna_gr_read_multi(a,t,scratch,stream,nullptr,0);else K::fused_gr_read_multi(a,t,scratch,stream);
   ck(cudaGetLastError());ck(cudaStreamSynchronize(stream));
   out[alt][0]=get(r,t*D);out[alt][1]=get(lo,t*L);out[alt][2]=get(rs,t*H);out[alt][3]=get(io,t*H);out[alt][4]=get(mix,t*N);
   if(q32_image&&!alt){K::native_quantize_q8_1(mix,qr,N,t,stream);ck(cudaStreamSynchronize(stream));qreference.resize(t*qstride);ck(cudaMemcpy(qreference.data(),qr,qreference.size(),cudaMemcpyDeviceToHost));}
  }
  if(q32_image){std::vector<uint8_t> image(qbytes+64);ck(cudaMemcpy(image.data(),qm,image.size(),cudaMemcpyDeviceToHost));bool equal=std::memcmp(image.data(),qreference.data(),qreference.size())==0,guard=std::all_of(image.begin()+t*qstride,image.end(),[](uint8_t v){return v==0xa5;});bad+=!equal||!guard;std::printf("Q32 T=%d apply=%d inject=%d image_bitwise=%d guard=%d\n",t,apply,in,equal,guard);}
  for(int f=0;f<5;++f){double ss=0,dd=0,mx=0;bool finite=true;
   // Upstream gr_up_q8_kernel holds lo in LDS and never publishes a.lo. Comparing
   // that workspace reads stale data; the independent A8 reference does define it.
   if(q8&&!a8&&f==1){std::printf("T=%d apply=%d inject=%d field=lo skipped=upstream-private-workspace\n",t,apply,in);continue;}
   for(size_t k=0;k<out[0][f].size();++k){double x=out[0][f][k],y=out[1][f][k];finite=finite&&std::isfinite(x)&&std::isfinite(y);ss+=x*x;dd+=(x-y)*(x-y);mx=std::fmax(mx,std::fabs(x-y));}
   double rel=std::sqrt(dd/std::fmax(ss,1e-30));bool ok=finite&&(q32?std::memcmp(out[0][f].data(),out[1][f].data(),out[0][f].size()*4)==0:rel<1e-4);bad+=!ok;
   std::printf("T=%d apply=%d inject=%d field=%s relative_l2=%.9g max_abs=%.9g finite=%d passed=%d\n",t,apply,in,names[f],rel,mx,finite,ok);
  }
  if(!apply&&(q32||(in&&(t==1||t==4))))for(int alt=0;alt<2;++alt){
   if(q32){K::rdna_gr_q32_probe_mode(alt);for(int k=0;k<t;++k)a[k].q8_mixed=alt&&q32_image?qm+k*qstride:nullptr;}
   auto run=[&]{if(q32&&alt){K::fused_gr_read_multi(a,t,scratch,stream);if(q32_image&&q32_policy_off)K::native_quantize_q8_1(mix,qm,N,t,stream);}else if(alt||q32)K::rdna_gr_read_multi(a,t,scratch,stream,nullptr,0);else K::fused_gr_read_multi(a,t,scratch,stream);};
   for(int z=0;z<20;++z)run();ck(cudaStreamSynchronize(stream));cudaEvent_t e0,e1;ck(cudaEventCreate(&e0));ck(cudaEventCreate(&e1));ck(cudaEventRecord(e0,stream));for(int z=0;z<200;++z)run();ck(cudaEventRecord(e1,stream));ck(cudaEventSynchronize(e1));float ms;ck(cudaEventElapsedTime(&ms,e0,e1));std::printf("BENCH T=%d alt=%d microseconds=%.4f\n",t,alt,ms*1000/200);ck(cudaEventDestroy(e0));ck(cudaEventDestroy(e1));
   if(alt){cudaGraph_t graph;cudaGraphExec_t exec;ck(cudaStreamBeginCapture(stream,cudaStreamCaptureModeThreadLocal));run();ck(cudaStreamEndCapture(stream,&graph));
    ck(cudaGraphInstantiate(&exec,graph,nullptr,nullptr,0));for(int z=0;z<3;++z)ck(cudaGraphLaunch(exec,stream));ck(cudaStreamSynchronize(stream));
    auto actual=get(mix,t*N);double ss=0,dd=0;bool finite=true;for(size_t i=0;i<actual.size();++i){double x=out[0][4][i],y=actual[i];finite=finite&&std::isfinite(x)&&std::isfinite(y);ss+=x*x;dd+=(x-y)*(x-y);}
    double rel=std::sqrt(dd/std::max(ss,1e-30));bool ok=finite&&(q32?std::memcmp(actual.data(),out[0][4].data(),actual.size()*4)==0:rel<1e-4);if(q32_image){std::vector<uint8_t> image(qreference.size());ck(cudaMemcpy(image.data(),qm,image.size(),cudaMemcpyDeviceToHost));ok=ok&&image==qreference;}bad+=!ok;std::printf("GRAPH T=%d relative_l2=%.9g passed=%d\n",t,rel,ok);ck(cudaGraphExecDestroy(exec));ck(cudaGraphDestroy(graph));}
  }
 }
 std::printf("SUMMARY bad=%d\n",bad);return bad?1:0;
}
