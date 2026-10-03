#!/usr/bin/env python3
"""Build LED previews from frames exported by the real C firmware algorithm."""
import argparse
import csv
import json
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
NAMES = ['启动／待机', '未连接', '连接中', '等待授权', '工作中', '思考中', '状态未更新', '成功', '等待输入', '失败', '取消']
NOTES = ['熄灭', '固定六灯 · 500 ms / 4 s', '双段四灯 · 100 ms 步进', '全环双闪 · 150 / 150 / 150 / 1550 ms',
         '连续六灯 · 150 ms 步进', '对向三灯 · 250 ms 步进', '六灯常亮 · 每 1.5 s 交换明暗',
         '四扇区依次点亮 · 2.4 s 周期', '固定八灯 · 600 ms / 2 s', '全环单闪 · 200 ms / 2.4 s', '固定八灯 · 300 ms / 3 s']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frames', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/design')
    args = parser.parse_args()
    frames = [[] for _ in NAMES]
    with args.frames.open() as source:
        for row in csv.DictReader(source):
            frames[int(row['mode'])].append([[int(row[f'{c}{i}']) for c in 'rgb'] for i in range(16)])
    assert all(len(movie) == 480 for movie in frames)
    args.output.mkdir(parents=True, exist_ok=True)
    html = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>AgentMate 灯环 v2</title>
<style>body{margin:32px;background:#10121d;color:#eee;font:15px system-ui}h1{font-size:24px}p{color:#aab0cb}
.controls{position:sticky;top:0;padding:16px;background:#10121df0;display:flex;gap:16px;flex-wrap:wrap;align-items:center}
button,input{accent-color:#36e29a}button{padding:7px 18px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(400px,1fr));gap:16px}
article{background:#191c2b;border-radius:16px;padding:16px}h2{font-size:18px;margin:0}canvas{width:100%;max-width:440px}
small{color:#aab0cb}</style><h1>AgentMate 灯环 v2</h1>
<p>实际 C 算法逐帧导出 · 16 颗独立灯点 · 稳定单向运动 · 固定明暗尾迹</p>
<p>左：独立灯点；右：少量扩散示意。预览不能复现材料透光率与真实 LED 色彩。</p>
<div class="controls"><button id="play">暂停</button><label>灯环亮度 <input id="brightness" type="range" min="0" max="100" value="30"><output id="percent">30%</output></label>
<label>速度 <select id="speed"><option value="1">1×</option><option value="0.5">0.5×</option><option value="2">2×</option></select></label>
<input id="seek" aria-label="动画时间" type="range" min="0" max="23950" step="50" value="0"><output id="time">0.00 s</output></div><div class="grid" id="grid"></div>
<script>const frames=__FRAMES__, names=__NAMES__, notes=__NOTES__;
const grid=document.getElementById('grid');
const contexts=names.map((name,i)=>{const card=document.createElement('article');card.innerHTML=`<h2>${name}</h2><p>${notes[i]}</p><canvas width="440" height="250"></canvas><small>独立灯点　　　　　　　　　轻度扩散</small>`;grid.append(card);return card.querySelector('canvas').getContext('2d');});
let running=true,elapsed=0,last=performance.now();
const play=document.getElementById('play'),brightness=document.getElementById('brightness'),seek=document.getElementById('seek'),speed=document.getElementById('speed');
play.onclick=()=>{running=!running;play.textContent=running?'暂停':'播放';};
seek.oninput=()=>{elapsed=Number(seek.value);};
function draw(ctx, pixels, percentage){ctx.clearRect(0,0,440,250);const scalar=Math.floor((percentage*255+50)/100);
 for(let view=0;view<2;view++){const x=110+220*view,y=125;ctx.strokeStyle='#35394d';ctx.lineWidth=16;ctx.beginPath();ctx.arc(x,y,82,0,2*Math.PI);ctx.stroke();
 pixels.forEach((pixel,i)=>{const a=i*Math.PI/8-Math.PI/2,px=x+82*Math.cos(a),py=y+82*Math.sin(a);const rgb=pixel.map(c=>Math.floor((c*scalar+127)/255));const color=`rgb(${rgb.join(',')})`;
 ctx.fillStyle=rgb.some(Boolean)?color:'#202332';ctx.shadowBlur=view&&rgb.some(Boolean)?12:0;ctx.shadowColor=color;ctx.beginPath();ctx.arc(px,py,view?8:6,0,2*Math.PI);ctx.fill();ctx.shadowBlur=0;});
 ctx.fillStyle='#aab0cb';ctx.font='12px system-ui';ctx.textAlign='center';ctx.fillText('0',x,y-105);}}
function tick(now){if(running)elapsed=(elapsed+(now-last)*Number(speed.value))%24000;last=now;seek.value=String(Math.floor(elapsed/50)*50);document.getElementById('time').value=(elapsed/1000).toFixed(2)+' s';document.getElementById('percent').value=brightness.value+'%';contexts.forEach((ctx,i)=>draw(ctx,frames[i][Math.floor(elapsed/50)],Number(brightness.value)));requestAnimationFrame(tick);}requestAnimationFrame(tick);
</script></html>'''
    for token, value in [('__FRAMES__', frames), ('__NAMES__', NAMES), ('__NOTES__', NOTES)]:
        html = html.replace(token, json.dumps(value, ensure_ascii=False, separators=(',', ':')))
    (args.output / 'led_patterns_v2.html').write_text(html)
    movies = []
    scalar = (30 * 255 + 50) // 100
    for pixels in frames[4][:48]:
        frame = Image.new('RGB', (440, 270), '#10121d')
        draw = ImageDraw.Draw(frame)
        draw.text((14, 8), 'Working / LED brightness 30%', fill='#cbd0e5')
        for view in range(2):
            cx, cy = 110 + 220 * view, 145
            draw.ellipse((cx-82, cy-82, cx+82, cy+82), outline='#35394d', width=14)
            glow = Image.new('RGB', frame.size)
            light = ImageDraw.Draw(glow)
            for i, pixel in enumerate(pixels):
                a = i * math.pi / 8 - math.pi / 2
                x, y = cx + 82 * math.cos(a), cy + 82 * math.sin(a)
                color = tuple((c * scalar + 127) // 255 for c in pixel)
                r = 8 if view else 6
                if view and any(color):
                    light.ellipse((x-r, y-r, x+r, y+r), fill=color)
                draw.ellipse((x-r, y-r, x+r, y+r), fill=color if any(color) else '#202332')
            if view:
                frame = ImageChops.add(frame, glow.filter(ImageFilter.GaussianBlur(5)))
                draw = ImageDraw.Draw(frame)
        draw.text((28, 250), 'Discrete points', fill='#aab0cb')
        draw.text((244, 250), 'Light diffusion', fill='#aab0cb')
        movies.append(frame)
    movies[0].save(args.output / 'led_working_v2.gif', save_all=True, append_images=movies[1:], duration=50, loop=0)
    movies[0].save(args.output / 'led_working_v2.png')
    print(args.output / 'led_patterns_v2.html')
    print(args.output / 'led_working_v2.gif')


if __name__ == '__main__':
    main()
