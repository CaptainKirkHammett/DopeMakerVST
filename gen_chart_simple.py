"""Generate a simplified signal-path chart for the in-plugin overlay.

Two-column layout: Left = MIDI/Voice/Pre-FX, Right = FX Chain/Output.
Trance Gate routing positions shown with labeled markers in the FX chain.
"""
from PIL import Image, ImageDraw, ImageFont

# ── Fonts ────────────────────────────────────────────────────────────────────
F_TITLE = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 22)
F_LABEL = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 16)
F_HDR   = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 13)
F_TINY  = ImageFont.truetype('C:/Windows/Fonts/arial.ttf',   11)

# ── Layout ───────────────────────────────────────────────────────────────────
COL_W   = 360                       # width of each column's box area
COL_PAD = 40                        # padding between columns
MARGIN  = 40                        # left/right margin
W       = MARGIN * 2 + COL_W * 2 + COL_PAD   # total width
BH      = 32
GAP     = 7

# Left column
L_BX = MARGIN
L_BW = COL_W
L_CX = L_BX + L_BW // 2

# Right column
R_BX = MARGIN + COL_W + COL_PAD
R_BW = COL_W
R_CX = R_BX + R_BW // 2

STYLE = {
    'midi'    : ('#0d2d45', '#2a7ab0', '#ffffff'),
    'engine'  : ('#1e1040', '#6a4ab0', '#ffffff'),
    'voice'   : ('#16082e', '#5a3aa0', '#e0d8ff'),
    'always'  : ('#0b2e1c', '#2a9a5a', '#ffffff'),
    'optional': ('#2e1a06', '#c07030', '#ffffff'),
    'utility' : ('#0b2e2e', '#2a9090', '#ffffff'),
    'output'  : ('#061828', '#1a5890', '#ffffff'),
    'gate'    : ('#2e0620', '#c03060', '#ffffff'),
}
BG = '#08080f'

H_LARGE = 3000
img = Image.new('RGB', (W, H_LARGE), BG)
d   = ImageDraw.Draw(img)


def tw(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[2] - bb[0]

def th(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[3] - bb[1]

def box(y, label, style, bx, bw, cx):
    bg, border, fg = STYLE[style]
    d.rounded_rectangle([bx, y, bx + bw, y + BH], radius=5, fill=bg, outline=border, width=2)
    lw = tw(label, F_LABEL)
    lh = th(label, F_LABEL)
    d.text((bx + (bw - lw) // 2, y + (BH - lh) // 2), label, fill=fg, font=F_LABEL)
    return y + BH

def lbox(y, label, style='always'):
    return box(y, label, style, L_BX, L_BW, L_CX)

def rbox(y, label, style='always'):
    return box(y, label, style, R_BX, R_BW, R_CX)

def hdr(y, text, cx):
    bx = cx - L_BW // 2
    bw = L_BW
    d.line([(bx, y + 10), (bx + bw, y + 10)], fill='#2a2a40', width=1)
    t_w = tw(text, F_HDR)
    tx = cx - t_w // 2
    d.rectangle([tx - 5, y + 3, tx + t_w + 5, y + 18], fill=BG)
    d.text((tx, y + 3), text, fill='#6080a0', font=F_HDR)
    return y + 24

def arr(y1, y2, cx):
    d.line([(cx, y1), (cx, y2)], fill='#4a7090', width=2)
    d.polygon([(cx - 4, y2 - 6), (cx + 4, y2 - 6), (cx, y2)], fill='#4a7090')


# ── TITLE ────────────────────────────────────────────────────────────────────
y = 14
title = 'Dope Maker VST \u2014 Signal Path'
d.text((W // 2 - tw(title, F_TITLE) // 2, y), title, fill='#b0d4ff', font=F_TITLE)
y_start = y + 42

# ══════════════════════════════════════════════════════════════════════════════
# LEFT COLUMN: MIDI & ROUTING → VOICE ENGINE → PRE-FX
# ══════════════════════════════════════════════════════════════════════════════
ly = y_start

# ── MIDI & ROUTING ───────────────────────────────────────────────────────────
ly = hdr(ly, 'MIDI & ROUTING', L_CX)

lbox(ly, 'MIDI Input', 'midi');               ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Gate Controller', 'midi');           ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Arpeggiator', 'midi');              ly += BH + GAP + 6

# ── VOICE ENGINE ─────────────────────────────────────────────────────────────
ly = hdr(ly, 'VOICE ENGINE', L_CX)
arr(ly - 2, ly + 3, L_CX)

lbox(ly, 'Sampler Engine', 'engine');          ly += BH + GAP + 4

# Voice frame
frame_top = ly
fi_bx = L_BX + 14
fi_bw = L_BW - 28
y_in = frame_top + 18
arr(y_in - 3, y_in + 2, L_CX)

def vbox(y_in, label):
    bg, border, fg = STYLE['voice']
    d.rounded_rectangle([fi_bx, y_in, fi_bx + fi_bw, y_in + BH], radius=5, fill=bg, outline=border, width=2)
    lw = tw(label, F_LABEL)
    lh = th(label, F_LABEL)
    d.text((fi_bx + (fi_bw - lw) // 2, y_in + (BH - lh) // 2), label, fill=fg, font=F_LABEL)
    return y_in + BH

vbox(y_in, 'Zero-Crossing Snap');        y_in += BH + GAP; arr(y_in - GAP, y_in, L_CX)
vbox(y_in, 'Sample Playback');           y_in += BH + GAP; arr(y_in - GAP, y_in, L_CX)
vbox(y_in, 'Pitch Env + Pitch LFO');     y_in += BH + GAP; arr(y_in - GAP, y_in, L_CX)
vbox(y_in, 'Volume Env + De-Click');     y_in += BH + 8

frame_bot = y_in + 2
d.rounded_rectangle([L_BX + 6, frame_top, L_BX + L_BW - 6, frame_bot],
                     radius=6, fill=None, outline='#4a3888', width=2)
fl = 'SamplerVoice \u00d7 N'
fl_tw = tw(fl, F_HDR)
fx = L_CX - fl_tw // 2
d.rectangle([fx - 4, frame_top - 1, fx + fl_tw + 4, frame_top + 13], fill=BG)
d.text((fx, frame_top + 1), fl, fill='#7060b0', font=F_HDR)

ly = frame_bot + GAP + 3
arr(ly - GAP, ly, L_CX)

lbox(ly, 'Voice Mix Sum', 'engine');           ly += BH + GAP + 6

# ── PRE-FX ───────────────────────────────────────────────────────────────────
ly = hdr(ly, 'PRE-FX', L_CX)
arr(ly - 2, ly + 3, L_CX)

lbox(ly, 'Highcut (Pre)', 'optional');         ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Lowcut (Pre)', 'optional');          ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Pre-Gain', 'always');                ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Normalize', 'optional');             ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Granulizer', 'optional');            ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Pre-FX VU Meter', 'utility');        ly += BH + GAP; arr(ly - GAP, ly, L_CX)
lbox(ly, 'Envelope Follower', 'utility');      ly += BH

left_bottom = ly

# ══════════════════════════════════════════════════════════════════════════════
# RIGHT COLUMN: FX CHAIN → OUTPUT
# ══════════════════════════════════════════════════════════════════════════════
ry = y_start

# ── FX CHAIN ─────────────────────────────────────────────────────────────────
ry = hdr(ry, 'FX CHAIN', R_CX)
arr(ry - 2, ry + 3, R_CX)

rbox(ry, 'Stereo Width', 'always');            ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Limiter', 'always');                 ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Gain', 'always');                    ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Sidechain \u2014 Pre-FX', 'optional'); ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Trance Gate \u2014 A: Pre FX', 'gate'); ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Stutter', 'optional');               ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Filter', 'optional');                ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Distortion', 'optional');            ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Flanger', 'optional');               ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Phaser', 'optional');                ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Trance Gate \u2014 B: Pre Reverb', 'gate'); ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Reverb', 'optional');                ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Ping-Pong Delay', 'optional');       ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Trance Gate \u2014 C: Post FX', 'gate'); ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Sidechain \u2014 Post-FX', 'optional'); ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Highcut (Post)', 'optional');        ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Lowcut (Post)', 'optional');         ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Clip', 'optional');                  ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'AutoEQ', 'optional');                ry += BH + GAP + 6

# ── OUTPUT ───────────────────────────────────────────────────────────────────
ry = hdr(ry, 'OUTPUT', R_CX)
arr(ry - 2, ry + 3, R_CX)

rbox(ry, 'Post-Gain', 'always');               ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Post-FX VU Meter', 'utility');       ry += BH + GAP; arr(ry - GAP, ry, R_CX)
rbox(ry, 'Audio Output', 'output');            ry += BH

right_bottom = ry

# ── Legend ────────────────────────────────────────────────────────────────────
final_y = max(left_bottom, right_bottom) + 40
leg_y = final_y - 24
d.line([(MARGIN, leg_y), (W - MARGIN, leg_y)], fill='#2a2a40', width=1)
items = [
    ('#2a7ab0', 'MIDI'),
    ('#6a4ab0', 'Voice'),
    ('#2a9090', 'Utility'),
    ('#2a9a5a', 'Always'),
    ('#c07030', 'Optional'),
    ('#c03060', 'Trance Gate'),
]
lx = MARGIN + 2
lgy = leg_y + 5
for col, label in items:
    d.rectangle([lx, lgy + 1, lx + 8, lgy + 9], fill=col)
    d.text((lx + 11, lgy), label, fill='#8090a0', font=F_TINY)
    lx += tw(label, F_TINY) + 22

# ── Save ─────────────────────────────────────────────────────────────────────
out = img.crop((0, 0, W, final_y))
path = r'C:\DevSuomiSamplerVST\signal_path_simple.png'
out.save(path)
print(f'Saved: {path}  ({W} x {final_y} px)')
