"""Local synchronized viewer for native UE before/after captures, not a web game."""
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from pathlib import Path
import argparse,re
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=5187);a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'unreal/Fireline/Saved'
html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M4 起停动作对照</title><style>body{margin:0;background:#101821;color:#e8eef5;font:16px system-ui;padding:24px}h1{font-size:24px}p{color:#b4c5d5;line-height:1.6}button,select{font:inherit;padding:9px 15px;background:#273a4e;color:white;border:1px solid #57728b;border-radius:6px;margin-right:10px}main{display:grid;grid-template-columns:1fr 1fr;gap:18px}img{width:100%;display:block;background:black}figure{margin:0}figcaption{padding:12px 0}input{width:min(700px,80%)}footer{margin-top:20px;font-size:14px;color:#adc0d0}@media(max-width:800px){main{grid-template-columns:1fr}}</style><h1>M4：待机 → 行走 → 停止</h1><p>左右是 Unreal 实际运行截图，按同一输入时序对照。仅检查本轮持枪层修正；不是所有动作的完成证明。</p><div><select id="angle" aria-label="观察角度"><option value="0">正面</option><option value="1" selected>侧面</option><option value="2">背面</option></select><button id="play">暂停</button><select id="speed" aria-label="播放速度"><option value="1">正常速度</option><option value="0.5">半速</option><option value="0.25">四分之一速度</option></select><span id="phase"></span></div><main><figure><figcaption>修正前：持枪层覆盖下半身</figcaption><img id="before" alt="修正前原生画面"></figure><figure><figcaption>修正后：保留步态，整套上身接到骨盆</figcaption><img id="after" alt="修正后原生画面"></figure></main><p><input id="time" aria-label="动作时间" type="range" min="0" max="19" step="1" value="0"><span id="label"></span></p><footer>每个视角约 2 秒，10 帧/秒采样。截图未包含帧间全部运动；游戏内仍需观察连续播放。测试前后固定输入；三个视角分别运行，不能假定不同视角的步态相位相同。</footer><script>
let frame=0,playing=true,elapsed=0,last=performance.now();const before=document.querySelector('#before'),after=document.querySelector('#after'),angle=document.querySelector('#angle'),time=document.querySelector('#time'),speed=document.querySelector('#speed');
function show(){const name=`transition-${String(angle.value).padStart(2,'0')}-${String(frame).padStart(2,'0')}.png`;before.src='/CarryLoopBefore/'+name;after.src='/CarryLoopCandidate/'+name;time.value=frame;document.querySelector('#label').textContent=(frame/10).toFixed(1)+' 秒';document.querySelector('#phase').textContent=frame<3?'无移动输入／恢复':frame<15?'行走／起步':'停止／收步'}
angle.onchange=()=>{frame=0;show()};time.oninput=()=>{frame=+time.value;playing=false;document.querySelector('#play').textContent='播放';show()};document.querySelector('#play').onclick=()=>{playing=!playing;document.querySelector('#play').textContent=playing?'暂停':'播放'};
function tick(now){if(playing){elapsed+=(now-last)*Number(speed.value);if(elapsed>=100){frame=(frame+Math.floor(elapsed/100))%20;elapsed%=100;show()}}last=now;requestAnimationFrame(tick)}show();requestAnimationFrame(tick);
</script></html>'''
class Handler(SimpleHTTPRequestHandler):
 def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(root),**kwargs)
 def do_GET(self):
  if self.path=='/':
   data=html.encode();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
  elif re.fullmatch(r'/CarryLoop(?:Before|Candidate)/transition-0[0-2]-[01][0-9]\.png',self.path):super().do_GET()
  else:self.send_error(404)
print(f'Review: http://127.0.0.1:{a.port}/',flush=True)
ThreadingHTTPServer(('127.0.0.1',a.port),Handler).serve_forever()
