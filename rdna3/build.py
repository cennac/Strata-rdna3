"""Build this checkout from source; no old object files or archives are reused."""
from pathlib import Path
import argparse,os,subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--rocm',required=True,type=Path)
 p.add_argument('--build-dir',type=Path,default=Path('build'))
 p.add_argument('--jobs',type=int,default=4)
 a=p.parse_args();root=Path(__file__).resolve().parents[1];rt=a.rocm.resolve();build=a.build_dir.resolve()
 compiler=rt/'llvm/bin/clang++'
 if not compiler.exists():compiler=rt/'lib/llvm/bin/clang++'
 if not compiler.exists():p.error('ROCm clang++ was not found')
 bitcode=rt/'lib/llvm/amdgcn/bitcode'
 if not bitcode.exists():bitcode=rt/'amdgcn/bitcode'
 env=os.environ.copy();env.update(HIP_PLATFORM='amd',HIP_COMPILER='clang',HIP_RUNTIME='rocclr',ROCM_PATH=str(rt),HIP_PATH=str(rt),PATH=str(rt/'bin')+':'+str(compiler.parent)+':'+env['PATH'],LD_LIBRARY_PATH=str(rt/'lib')+':'+str(rt/'lib/llvm/lib'))
 cmd=['cmake','-S',str(root/'engine'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DSTRATA_ENABLE_HIP=ON','-DSTRATA_ENABLE_CUDA=OFF','-DSTRATA_BUILD_TESTS=OFF','-DSTRATA_BUILD_CONVERSATION_TESTS=OFF','-DCMAKE_HIP_ARCHITECTURES=gfx1100','-DCMAKE_HIP_COMPILER='+str(compiler),'-DCMAKE_HIP_COMPILER_ROCM_ROOT='+str(rt),'-DCMAKE_HIP_FLAGS=--rocm-path='+str(rt)+' --rocm-device-lib-path='+str(bitcode),'-DCMAKE_PREFIX_PATH='+str(rt)]
 # The legacy engine uses pinned GGML and rocWMMA sources. The 0.2.16 engine has its own kernels.
 llama=root/'third_party/llama.cpp';wmma=root/'third_party/rocwmma/library/include'
 if llama.exists():cmd+=['-DSTRATA_GGML_DIR='+str(llama),'-DSTRATA_PREFILL_MMQ=ON','-DSTRATA_MMQ_KQUANTS=ON','-DSTRATA_ROCWMMA_INCLUDE_DIR='+str(wmma)]
 subprocess.run(cmd,env=env,check=True)
 subprocess.run(['cmake','--build',str(build),'--target','strata','-j',str(a.jobs)],env=env,check=True)
 print(build/'strata')

if __name__=='__main__':main()
