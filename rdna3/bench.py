"""Public short-prompt benchmark: single request and two concurrent requests."""
import argparse,json,time,threading,statistics,urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PROMPTS={
 'zh':'请编写一份完整、详细的本地大模型运维手册，必须逐项写满120个编号条目，每项包含一条具体操作和验证方法，依次涵盖模型文件校验、显存、KV缓存、并发、批处理、PCIe、内存、日志、容器、API、备份、监控与故障恢复。不要省略条目，不要提前总结。',
 'code':'Write a complete production Python HTTP job service of at least 500 lines using only the standard library. Include dataclasses, persistence with SQLite, retry/backoff, thread workers, bounded queues, cancellation, deadlines, CLI, JSON HTTP routes, metrics and unittest tests. Output the full code without abbreviations or explanation.'}
def main():
 p = argparse.ArgumentParser(description=__doc__)
 p.add_argument('--url', default='http://127.0.0.1:8101')
 p.add_argument('--model', required=True)
 p.add_argument('--output', required=True, type=Path)
 p.add_argument('--repeats', type=int, default=3)
 p.add_argument('--tokens', type=int, default=1024)
 p.add_argument('--first-code-dual', action='store_true',
                help='measure code dual before and after the other workloads (use --repeats 1 per fresh engine)')
 a = p.parse_args()
 if a.output.exists():p.error('Output exists; choose another filename')
 def request(kind,lane,barrier,tokens):
  body={'model':a.model,'messages':[{'role':'user','content':f'任务编号{lane}。\n'+PROMPTS[kind]}],'max_tokens':tokens,'temperature':0,'top_k':1,'top_p':1,'seed':42,'presence_penalty':1.5,'frequency_penalty':0,'cache_prompt':False,'reasoning_effort':'none','chat_template_kwargs':{'enable_thinking':False},'stream':True,'stream_options':{'include_usage':True}}
  req=urllib.request.Request(a.url.rstrip('/')+'/v1/chat/completions',data=json.dumps(body,ensure_ascii=False).encode(),headers={'Content-Type':'application/json'})
  barrier.wait();start=time.perf_counter();first=None;text=[];timings={};usage={};finish=None
  with urllib.request.urlopen(req,timeout=300) as f:
   for line in f:
    if not line.startswith(b'data:'):continue
    raw=line[5:].strip()
    if raw==b'[DONE]':continue
    x=json.loads(raw)
    if x.get('error'):raise RuntimeError(str(x['error']))
    timings=x.get('timings') or timings;usage=x.get('usage') or usage
    for ch in x.get('choices',[]):
     d=ch.get('delta') or {};s=d.get('content') or d.get('reasoning_content') or ''
     if s:first=first or time.perf_counter();text.append(s)
     finish=ch.get('finish_reason') or finish
  end=time.perf_counter();return {'start':start,'end':end,'ttft_s':None if first is None else first-start,'wall_s':end-start,'tokens':usage.get('completion_tokens',0),'timings':timings,'usage':usage,'finish':finish,'text':''.join(text),'length_matched':usage.get('completion_tokens')==tokens}
 result={'settings':vars(a)|{'output':str(a.output)},'prompts':PROMPTS,'warmup':request('zh',0,threading.Barrier(1),128),'bursts':[]}
 for rep in range(a.repeats):
  if a.first_code_dual:
   tasks = [('code', 2), ('zh', 1), ('zh', 2), ('code', 1), ('code-warm', 2)]
  else:
   tasks = [('zh', 1), ('zh', 2), ('code', 1), ('code', 2)]
   if rep % 2:
    tasks.reverse()
  for kind, n in tasks:
   barrier=threading.Barrier(n)
   prompt = kind.removesuffix('-warm')
   with ThreadPoolExecutor(n) as pool:rows=list(pool.map(lambda lane:request(prompt,lane,barrier,a.tokens),range(n)))
   burst={'kind':kind,'n':n,'repeat':rep,'rows':rows,'length_matched':all(r['length_matched'] for r in rows),'aggregate_tps':sum(r['tokens'] for r in rows)/(max(r['end'] for r in rows)-min(r['start'] for r in rows))}
   result['bursts'].append(burst);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
   print(kind,n,rep,round(burst['aggregate_tps'],2),'length_matched',burst['length_matched'],flush=True)
 print('This records short prompts. Context capacity is not an actual long-input measurement.')
if __name__=='__main__':main()
