"""Generate a simplified signal-path chart for the in-plugin overlay.

Only block names — no parameter details or sub-labels.
Larger fonts, tighter layout, designed to be readable at VST window size.
"""
from PIL import Image, ImageDraw, ImageFont

# ── Fonts ────────────────────────────────────────────────────────────────────
F_TITLE = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 22)
F_LABEL = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 16)
F_HDR   = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 13)
F_TINY  = ImageFont.truetype('C:/Windows/Fonts/arial.ttf',   11)

# ── Layout ───────────────────────────────────────────────────────────────────
W   = 480
BX  = 60
BW  = 360
BH  = 32
GAP = 7
CX  = BX + BW // 2

STYLE = {
    'midi'    : ('#0d2d45', '#2a7ab0', '#ffffff'),
    'engine'  : ('#1e1040', '#6a4ab0', '#ffffff'),
    'voice'   : ('#16082e', '#5a3aa0', '#e0d8ff'),
    'always'  : ('#0b2e1c', '#2a9a5a', '#ffffff'),
    'optional': ('#2e1a06', '#c07030', '#ffffff'),
    'utility' : ('#0b2e2e', '#2a9090', '#ffffff'),
    'output'  : ('#061828', '#1a5890', '#ffffff'),
}
BG = '#08080f'

H_LARGE = 4000
img = Image.new('RGB', (W, H_LARGE), BG)
d   = ImageDraw.Draw(img)


def tw(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[2] - bb[0]

def th(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[3] - bb[1]

def box(y, label, style='always'):
    bg, border, fg = STYLE[style]
    d.rounded_rectangle([BX, y, BX + BW, y + BH], radius=5, fill=bg, outline=border, width=2)
    lw = tw(label, F_LABEL)
    lh = th(label, F_LABEL)
    d.text((BX + (BW - lw) // 2, y + (BH - lh) // 2), label, fill=fg, font=F_LABEL)
    return y + BH

def hdr(y, text):
    d.line([(BX, y + 10), (BX + BW, y + 10)], fill='#2a2a40', width=1)
    t_w = tw(text, F_HDR)
    tx = CX - t_w // 2
    d.rectangle([tx - 5, y + 3, tx + t_w + 5, y + 18], fill=BG)
    d.text((tx, y + 3), text, fill='#6080a0', font=F_HDR)
    return y + 24

def arr(y1, y2):
    d.line([(CX, y1), (CX, y2)], fill='#4a7090', width=2)
    d.polygon([(CX - 4, y2 - 6), (CX + 4, y2 - 6), (CX, y2)], fill='#4a7090')


# ── TITLE ────────────────────────────────────────────────────────────────────
y = 14
title = 'Dope Maker VST \u2014 Signal Path'
d.text((W // 2 - tw(title, F_TITLE) // 2, y), title, fill='#b0d4ff', font=F_TITLE)
y += 36

# ── MIDI & ROUTING ───────────────────────────────────────────────────────────
y = hdr(y, 'MIDI & ROUTING')

box(y, 'MIDI Input', 'midi');            y += BH + GAP; arr(y - GAP, y)
box(y, 'Gate Controller', 'midi');       y += BH + GAP; arr(y - GAP, y)
box(y, 'Arpeggiator', 'midi');           y += BH + GAP + 6

# ── VOICE ENGINE ─────────────────────────────────────────────────────────────
y = hdr(y, 'VOICE ENGINE')
arr(y - 2, y + 3)

box(y, 'Sampler Engine', 'engine');      y += BH + GAP + 4

# Voice frame
frame_top = y
fi_bx = BX + 14
fi_bw = BW - 28
y_in = frame_top + 18
arr(y_in - 3, y_in + 2)

def vbox(y_in, label):
    bg, border, fg = STYLE['voice']
    d.rounded_rectangle([fi_bx, y_in, fi_bx + fi_bw, y_in + BH], radius=5, fill=bg, outline=border, width=2)
    lw = tw(label, F_LABEL)
    lh = th(label, F_LABEL)
    d.text((fi_bx + (fi_bw - lw) // 2, y_in + (BH - lh) // 2), label, fill=fg, font=F_LABEL)
    return y_in + BH

vbox(y_in, 'Zero-Crossing Snap');        y_in += BH + GAP; arr(y_in - GAP, y_in)
vbox(y_in, 'Sample Playback');           y_in += BH + GAP; arr(y_in - GAP, y_in)
vbox(y_in, 'Pitch Env + Pitch LFO');     y_in += BH + GAP; arr(y_in - GAP, y_in)
vbox(y_in, 'Volume Env + De-Click');     y_in += BH + 8

frame_bot = y_in + 2
d.rounded_rectangle([BX + 6, frame_top, BX + BW - 6, frame_bot],
                     radius=6, fill=None, outline='#4a3888', width=2)
fl = 'SamplerVoice \u00d7 N'
fl_tw = tw(fl, F_HDR)
fx = CX - fl_tw // 2
d.rectangle([fx - 4, frame_top - 1, fx + fl_tw + 4, frame_top + 13], fill=BG)
d.text((fx, frame_top + 1), fl, fill='#7060b0', font=F_HDR)

y = frame_bot + GAP + 3
arr(y - GAP, y)

box(y, 'Voice Mix Sum', 'engine');       y += BH + GAP + 6

# ── PRE-FX ───────────────────────────────────────────────────────────────────
y = hdr(y, 'PRE-FX')
arr(y - 2, y + 3)

box(y, 'Pre-FX VU Meter', 'utility');    y += BH + GAP; arr(y - GAP, y)
box(y, 'Envelope Follower', 'utility');  y += BH + GAP + 6

# ── FX CHAIN ─────────────────────────────────────────────────────────────────
y = hdr(y, 'FX CHAIN')
arr(y - 2, y + 3)

box(y, 'Stereo Width', 'always');        y += BH + GAP; arr(y - GAP, y)
box(y, 'Limiter', 'always');             y += BH + GAP; arr(y - GAP, y)
box(y, 'Gain', 'always');                y += BH + GAP; arr(y - GAP, y)
box(y, 'Sidechain \u2014 Pre-FX', 'optional'); y += BH + GAP; arr(y - GAP, y)
box(y, 'Filter', 'optional');            y += BH + GAP; arr(y - GAP, y)
box(y, 'Distortion', 'optional');        y += BH + GAP; arr(y - GAP, y)
box(y, 'Flanger', 'optional');           y += BH + GAP; arr(y - GAP, y)
box(y, 'Phaser', 'optional');            y += BH + GAP; arr(y - GAP, y)
box(y, 'Ping-Pong Delay', 'optional');   y += BH + GAP; arr(y - GAP, y)
box(y, 'Sidechain \u2014 Post-FX', 'optional'); y += BH + GAP; arr(y - GAP, y)
box(y, 'Highcut', 'optional');           y += BH + GAP; arr(y - GAP, y)
box(y, 'Lowcut', 'optional');            y += BH + GAP; arr(y - GAP, y)
box(y, 'Clip', 'optional');              y += BH + GAP + 6

# ── OUTPUT ───────────────────────────────────────────────────────────────────
y = hdr(y, 'OUTPUT')
arr(y - 2, y + 3)

box(y, 'Post-FX VU Meter', 'utility');   y += BH + GAP; arr(y - GAP, y)
box(y, 'Audio Output', 'output');        y += BH

final_y = y + 34

# ── Legend ────────────────────────────────────────────────────────────────────
leg_y = final_y - 24
d.line([(BX, leg_y), (BX + BW, leg_y)], fill='#2a2a40', width=1)
items = [
    ('#2a7ab0', 'MIDI'),
    ('#6a4ab0', 'Voice'),
    ('#2a9090', 'Utility'),
    ('#2a9a5a', 'Always'),
    ('#c07030', 'Optional'),
]
lx = BX + 2
ly = leg_y + 5
for col, label in items:
    d.rectangle([lx, ly + 1, lx + 8, ly + 9], fill=col)
    d.text((lx + 11, ly), label, fill='#8090a0', font=F_TINY)
    lx += tw(label, F_TINY) + 22

# ── Save ─────────────────────────────────────────────────────────────────────
out = img.crop((0, 0, W, final_y))
path = r'C:\DevSuomiSamplerVST\signal_path_simple.png'
out.save(path)
print(f'Saved: {path}  ({W} x {final_y} px)')
