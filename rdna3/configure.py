"""Write a local config from the public template; this never starts a service."""
from pathlib import Path
import argparse,json

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['pack','native','ple-gguf','mtp','rocm']:p.add_argument('--'+name,required=True,type=Path)
 p.add_argument('--gpu',default='0,1',help='HIP device ordinals, not Vulkan indices')
 p.add_argument('--output',type=Path,default=Path('state/dual-xxs.json'))
 a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve()
 if out.exists():p.error('Output exists; choose another filename')
 values={k.upper():str(getattr(a,k).resolve()) for k in ['pack','native','mtp','rocm']};values['PLE_GGUF']=str(a.ple_gguf.resolve());values.update(ROOT=str(root),STATE=str(out.parent))
 for k in ['PACK','NATIVE','PLE_GGUF','MTP','ROCM']:
  if not Path(values[k]).exists():p.error(k+' does not exist: '+values[k])
 gpu=[int(s) for s in a.gpu.split(',')]
 if len(gpu)!=2 or len(set(gpu))!=2:p.error('This profile needs two distinct HIP GPU ordinals')
 cfg=json.loads((root/'rdna3/dual-xxs.template.json').read_text(encoding='utf-8'))
 def fill(x):
  if isinstance(x,str):
   for k,v in values.items():x=x.replace('@'+k+'@',v)
   return x
  if isinstance(x,list):return [fill(v) for v in x]
  if isinstance(x,dict):return {k:fill(v) for k,v in x.items()}
  return x
 cfg=fill(cfg);cfg['gpu']=gpu;cfg['env']['HIP_VISIBLE_DEVICES']=','.join(map(str,gpu))
 out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(out)
if __name__=='__main__':main()
