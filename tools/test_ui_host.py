#!/usr/bin/env python3
"""Compile the actual UI renderer with mocked hardware and verify policy and pixels."""
import subprocess
from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=ROOT/'build'/'ui_host')
    args=p.parse_args(); out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    (out/'animation').mkdir(exist_ok=True)
    version = re.search(r'project\(agentmate_esp32 VERSION (\d+\.\d+)\)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    fonts=ROOT/'components/lcd_font'
    cmd=['cc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=address,undefined',
         '-D_POSIX_C_SOURCE=200809L',f'-DAM_SOFTWARE_VERSION="v{version}"',
         '-DCONFIG_LCD_FONT_ASCII_10X18=1','-DCONFIG_LCD_FONT_SUN_ASCII_12X22=1','-DCONFIG_LCD_FONT_TER_ASCII_16X32=1',
         '-I'+str(ROOT/'tests/stubs'),'-I'+str(ROOT/'main'),'-I'+str(fonts),
         str(ROOT/'tests/test_ui.c'), str(ROOT/'tests/test_led.c'),*[str(ROOT/'main'/f) for f in ['agentmate_status.c','agentmate_led.c','agentmate_usage.c','agentmate_text.c','agentmate_ui_state.c','agentmate_ui_render.c','agentmate_ui_assets.c']],
         str(fonts/'lcd_fonts.c'),
         '-lm','-o',str(out/'test_ui')]
    subprocess.run(cmd,check=True,cwd=ROOT); subprocess.run([str(out/'test_ui'),str(out)],check=True)
    try:
        from PIL import Image,ImageDraw
    except ImportError:
        print('PPM previews ready; install Pillow for PNG contact sheet'); return
    files=sorted(out.glob('*.ppm')); sheet=Image.new('RGB',(4*256,((len(files)+3)//4)*278),'#ececf4'); draw=ImageDraw.Draw(sheet)
    for i,file in enumerate(files):
        image=Image.open(file); image.save(out/(file.stem+'.png'))
        x=(i%4)*256;y=(i//4)*278;sheet.paste(image,(x+8,y+6));draw.text((x+8,y+248),file.stem,fill='#242444')
    sheet.save(out/'firmware_preview.png'); print(out/'firmware_preview.png')
    examples = [('working', 'Working / square timer'), ('waiting_for_input', 'Waiting for input'),
                ('permission_required', 'Approval required'), ('session_duration_1000', 'Four-digit minutes'),
                ('session_context_k', 'Context / K'), ('session_context_large', 'Context / M'),
                ('session_quota_dual', '5H + WEEK'), ('session_quota_week', 'WEEK only')]
    session_sheet = Image.new('RGB', (1024, 556), '#edf2ef'); draw = ImageDraw.Draw(session_sheet)
    for i, (stem, title) in enumerate(examples):
        x, y = i % 4 * 256, i // 4 * 278
        session_sheet.paste(Image.open(out / (stem + '.ppm')), (x + 8, y + 6))
        draw.text((x + 8, y + 250), title, fill='#233e34')
    session_sheet.save(out / 'session_preview.png')
    pages = ['boot', 'wait_host', 'handshake', 'syncing', 'idle', 'idle_unknown', 'usage', 'usage_week_single',
             'usage_unknown', 'usage_extremes', 'info', 'info_unknown',
             'factory_home', 'factory_led_single', 'factory_backlight', 'factory_font_4']
    page_sheet = Image.new('RGB', (1024, 1112), '#edf2ef'); draw = ImageDraw.Draw(page_sheet)
    for i, stem in enumerate(pages):
        x, y = i % 4 * 256, i // 4 * 278
        page_sheet.paste(Image.open(out / (stem + '.ppm')), (x + 8, y + 6))
        draw.text((x + 8, y + 250), stem, fill='#233e34')
    page_sheet.save(out / 'pages_preview.png')
    clock_frames = [Image.open(out / ('idle_colon_' + phase + '.ppm')).convert('RGB') for phase in ('on', 'off')]
    clock_frames[0].save(out / 'idle_clock_blink.gif', save_all=True, append_images=clock_frames[1:], duration=500, loop=0)
    states = ['working','thinking','waiting_for_input','permission_required','stale','task_success','task_failed','task_canceled']
    movies = [[Image.open(path).convert('RGB') for path in sorted((out/'animation').glob(state+'_*.ppm'))] for state in states]
    atlas = Image.new('RGB',(40*145,8*56),'#141733')
    for row, movie in enumerate(movies):
        for column, frame in enumerate(movie):
            atlas.paste(frame.crop((77,86,222,142)),(column*145,row*56))
    atlas.save(out/'status_border_frames.png')
    frames = []
    for tick in range(120):
        frame = Image.new('RGB',(4*256,2*278),'#ececf4'); draw = ImageDraw.Draw(frame)
        for i, movie in enumerate(movies):
            x = i % 4 * 256; y = i // 4 * 278
            view = movie[0].copy(); view.paste(movie[tick % len(movie)].crop((77,86,222,142)),(77,86))
            frame.paste(view,(x+8,y+6)); draw.text((x+8,y+248),states[i],fill='#242444')
        frames.append(frame)
    frames[0].save(out/'session_status_motion.gif',save_all=True,append_images=frames[1:],duration=100,loop=0)
    print(out/'session_status_motion.gif')

if __name__=='__main__':main()
