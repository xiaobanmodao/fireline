"""Local native-capture review. Only the selected PNGs are served."""
import json
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
root=Path(__file__).resolve().parents[1]/'unreal/Fireline/Saved'
folders=['SourceArmActionsBefore','MP7CorrectionActions','SourceArmBefore','MP7CorrectionMovementFinal']
cases=[];allowed={}
def add(label,a,b,frames,times):
 entry={'label':label,'frames':frames,'times':times,'paths':[]}
 for folder in (a,b):
  paths=[]
  for file in frames:
   p=root/folder/file
   if not p.is_file():raise RuntimeError(str(p))
   url='/'+folder+'/'+file;allowed[url]=p;paths.append(url)
  entry['paths'].append(paths)
 cases.append(entry)
a,b=folders[:2]
for label,slot in [('M4 换弹连续片段','1'),('MP7 换弹连续片段','3'),('爪刀检视连续片段','2')]:
 frames=sorted(str(p.relative_to(root/a)) for p in (root/a/'motion').glob(slot+'-*.png') if (root/b/'motion'/p.name).exists())
 ticks=[int(Path(x).stem.split('-')[1])/60 for x in frames]
 add(label,a,b,frames,[round(t-ticks[0],5) for t in ticks])
for weapon,title in [('m4','M4'),('mp7','MP7'),('knife','爪刀')]:
 for name,label in [('hold-front','持械正面'),('hold-left','侧面'),('hold-back','背面'),('ads-level','平视瞄准'),('ads-up','向上瞄准'),('ads-down','向下瞄准'),('crouch','下蹲'),('ads-up-89','极端向上（仍需修正）'),('ads-down-89','极端向下（仍需修正）'),('slide','滑铲'),('jump','跳跃')]:
  file=weapon+'-'+name+'.png'
  if (root/a/file).exists() and (root/b/file).exists():add(title+' · '+label,a,b,[file],[0])
for group,title in [(2,'M4 侧向行走'),(6,'M4 反向侧移'),(10,'M4 侧向跑动'),(14,'M4 反向侧跑'),(16,'M4 快速左右反向'),(18,'MP7 左右反向'),(19,'MP7 前后反向')]:
 files=[f'transition-{group:02d}-{i:02d}.png' for i in range(20)];add(title,folders[2],folders[3],files,[i/10 for i in range(20)])
html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>人物动作检查</title><style>body{margin:24px;background:#111b25;color:#e6edf4;font:16px system-ui}h1{font-size:25px}p{color:#b9c7d5;line-height:1.6}select,button{font:inherit;padding:10px;background:#26394a;color:white;border:1px solid #607489;border-radius:5px;margin-right:12px}main{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin-top:20px}figure{margin:0}img{width:100%;display:block}figcaption{margin:10px 0}input{width:70%}@media(max-width:850px){main{grid-template-columns:1fr}}</style><h1>人物动作 · 原生画面对照</h1><p>仅保留连续性修正。原始动画、模型与抓握未替换。极端俯仰及部分第三人称换弹尚未完成；这不是“全套动作已验收”的展示。</p><select id="case" aria-label="动作"></select><button id="play">暂停</button><select id="speed" aria-label="速度"><option value="1">正常速度</option><option value="0.5">半速</option><option value="0.25">四分之一速度</option></select><main><figure><figcaption>修正前</figcaption><img id="a" alt="修正前"></figure><figure><figcaption>保留的连续性修正</figcaption><img id="b" alt="修正后"></figure></main><p><input type="range" id="time" aria-label="动作进度" min="0" max="1" step="0.001"><span id="clock"></span></p><p>这是 Unreal 实际运行截图序列，原生动作片段按采样时间播放。检查完整操作请进入桌面开发版；数值通过不能替代最终网格检查。</p><script>const cases=DATA;const choose=document.querySelector('#case'),play=document.querySelector('#play'),slider=document.querySelector('#time'),speed=document.querySelector('#speed');for(let i=0;i<cases.length;i++){let o=document.createElement('option');o.value=i;o.textContent=cases[i].label;choose.append(o)}choose.value=String(cases.findIndex(c=>c.label==='MP7 左右反向'));let t=0,running=true,last=performance.now(),old=-1;function draw(){let c=cases[+choose.value];let i=0;while(i+1<c.times.length&&c.times[i+1]<=t)i++;if(i!==old){document.querySelector('#a').src=c.paths[0][i];document.querySelector('#b').src=c.paths[1][i];old=i}slider.value=t;document.querySelector('#clock').textContent=t.toFixed(2)+' s'}function reset(){t=0;old=-1;slider.max=cases[+choose.value].times.at(-1)+.05;draw()}choose.onchange=reset;play.onclick=()=>{running=!running;play.textContent=running?'暂停':'播放'};slider.oninput=()=>{running=false;play.textContent='播放';t=+slider.value;draw()};function tick(now){if(running&&cases[+choose.value].frames.length>1){t=(t+(now-last)/1000*+speed.value)%+slider.max;draw()}last=now;requestAnimationFrame(tick)}reset();requestAnimationFrame(tick);</script></html>'''.replace('DATA',json.dumps(cases,ensure_ascii=False))
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path=='/':data=html.encode();mime='text/html; charset=utf-8'
  elif self.path in allowed:data=allowed[self.path].read_bytes();mime='image/png'
  else:self.send_error(404);return
  self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def log_message(self,*args):pass
print('Review: http://127.0.0.1:5188/',flush=True)
ThreadingHTTPServer(('127.0.0.1',5188),Handler).serve_forever()
