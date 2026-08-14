from PIL import Image, ImageDraw, ImageFont

# ── Scale factor (3 = print quality at 300 DPI) ─────────────────────────────
S = 3

# ── Fonts (sizes scaled) ─────────────────────────────────────────────────────
F_TITLE = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 17 * S)
F_LABEL = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 14 * S)
F_SUB   = ImageFont.truetype('C:/Windows/Fonts/arial.ttf',   10 * S)
F_HDR   = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 11 * S)
F_TINY  = ImageFont.truetype('C:/Windows/Fonts/arial.ttf',    9 * S)

# ── Layout (all pixel values scaled) ─────────────────────────────────────────
W   = 870 * S
BX  = 160 * S
BW  = 540 * S
BH  =  50 * S
GAP =  11 * S
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

H_LARGE = 9000
img = Image.new('RGB', (W, H_LARGE), BG)
d   = ImageDraw.Draw(img)


def tw(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[2] - bb[0]

def th(txt, font):
    bb = d.textbbox((0, 0), txt, font=font)
    return bb[3] - bb[1]

def box(y, label, sub1=None, sub2=None, style='always', bx=None, bw=None, h=None):
    x  = bx if bx is not None else BX
    w  = bw if bw is not None else BW
    bh = h  if h  is not None else BH
    bg, border, fg = STYLE[style]
    d.rounded_rectangle([x, y, x+w, y+bh], radius=6*S, fill=bg, outline=border, width=2*S)
    lines = [(label, F_LABEL, fg)]
    if sub1:
        lines.append((sub1, F_SUB, '#90b8d0'))
    if sub2:
        lines.append((sub2, F_SUB, '#90b8d0'))
    total = sum(th(ln, fn) for ln, fn, _ in lines) + max(0, (len(lines)-1) * 3*S)
    ty = y + (bh - total) // 2
    for txt, fn, col in lines:
        d.text((x + (w - tw(txt, fn))//2, ty), txt, fill=col, font=fn)
        ty += th(txt, fn) + 3*S
    return y + bh

def hdr(y, text):
    d.line([(BX, y + 14*S), (BX+BW, y + 14*S)], fill='#2a2a40', width=1*S)
    t_w = tw(text, F_HDR)
    tx = CX - t_w // 2
    d.rectangle([tx - 6*S, y + 6*S, tx + t_w + 6*S, y + 22*S], fill=BG)
    d.text((tx, y + 6*S), text, fill='#6080a0', font=F_HDR)
    return y + 30*S

def arr_v(x, y1, y2, col='#4a7090'):
    d.line([(x, y1), (x, y2)], fill=col, width=2*S)
    d.polygon([(x - 5*S, y2 - 7*S), (x + 5*S, y2 - 7*S), (x, y2)], fill=col)

def arr_h(x1, x2, y, col, dashed=False):
    if dashed:
        cx = x1
        while cx < x2 - 8*S:
            d.line([(cx, y), (min(cx + 5*S, x2), y)], fill=col, width=1*S)
            cx += 9*S
    else:
        d.line([(x1, y), (x2, y)], fill=col, width=1*S)
    if x2 > x1:
        d.polygon([(x2 - 6*S, y - 3*S), (x2 - 6*S, y + 3*S), (x2, y)], fill=col)
    else:
        d.polygon([(x2 + 6*S, y - 3*S), (x2 + 6*S, y + 3*S), (x2, y)], fill=col)


# ── TITLE ────────────────────────────────────────────────────────────────────
y = 18 * S
title = 'Dope Maker VST \u2014 Signal Path'
d.text((W//2 - tw(title, F_TITLE)//2, y), title, fill='#b0d4ff', font=F_TITLE)
y += 38 * S

# ── MIDI & ROUTING ───────────────────────────────────────────────────────────
y = hdr(y, 'MIDI & ROUTING')

box(y, 'MIDI Input', style='midi')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Gate Controller', 'Step Sequencer  \u00b7  Block-rate amplitude envelope', style='midi')
gate_mid = y + BH // 2
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Arpeggiator', style='midi')
y += BH + GAP + 10*S

# ── VOICE ENGINE ─────────────────────────────────────────────────────────────
y = hdr(y, 'VOICE ENGINE')
arr_v(CX, y - 2*S, y + 4*S)

mod_x = BX - 108*S
mod_w = 92*S

lfo_sp_y = y + 4*S
d.rounded_rectangle([mod_x, lfo_sp_y, mod_x+mod_w, lfo_sp_y + 36*S],
                    radius=4*S, fill='#20103a', outline='#6040a0', width=1*S)
d.text((mod_x + 4*S, lfo_sp_y + 4*S),  'LFO',          fill='#c0a0ff', font=F_TINY)
d.text((mod_x + 4*S, lfo_sp_y + 15*S), 'Start Point',  fill='#c0a0ff', font=F_TINY)
d.text((mod_x + 4*S, lfo_sp_y + 26*S), 'Offset',       fill='#c0a0ff', font=F_TINY)

vel_y = lfo_sp_y + 42*S
d.rounded_rectangle([mod_x, vel_y, mod_x+mod_w, vel_y + 36*S],
                    radius=4*S, fill='#20103a', outline='#6040a0', width=1*S)
d.text((mod_x + 4*S, vel_y + 4*S),  'Velocity LFO',   fill='#c0a0ff', font=F_TINY)
d.text((mod_x + 4*S, vel_y + 15*S), '+ Probability',  fill='#c0a0ff', font=F_TINY)
d.text((mod_x + 4*S, vel_y + 26*S), 'Gate',           fill='#c0a0ff', font=F_TINY)

box(y, 'SamplerEngine', 'Voice Allocation  \u00b7  Slicer  \u00b7  Note Stealing  \u00b7  Portamento', style='engine')
arr_h(mod_x + mod_w, BX, lfo_sp_y + 18*S, '#6040a0', dashed=True)
arr_h(mod_x + mod_w, BX, vel_y    + 18*S, '#6040a0', dashed=True)
y += BH + GAP + 8*S

# Voice frame
frame_top = y
fi_bx = BX + 22*S
fi_bw = BW - 44*S
y_in  = frame_top + 22*S
arr_v(CX, y_in - 4*S, y_in + 2*S)

box(y_in, 'Zero-Crossing Snap',
    'Start point snapped to nearest ZC within \u00b11\u202f024 frames per channel',
    style='voice', bx=fi_bx, bw=fi_bw)
y_in += BH + GAP
arr_v(CX, y_in - GAP, y_in)

box(y_in, 'Sample Playback',
    'Linear interpolation  \u00b7  Forward / Reverse / Ping-Pong loop',
    style='voice', bx=fi_bx, bw=fi_bw)
y_in += BH + GAP
arr_v(CX, y_in - GAP, y_in)

box(y_in, 'Pitch Envelope ADSR  +  Pitch LFO',
    'Playback ratio = exp((note + bend + pitchEnv + pitchLFO) \u00d7 0.057762)  \u00b7  Portamento glide',
    style='voice', bx=fi_bx, bw=fi_bw)
y_in += BH + GAP
arr_v(CX, y_in - GAP, y_in)

box(y_in, 'Volume Envelope ADSR  \u00b7  De-Click',
    'gain = volumeEnv \u00d7 deClick \u00d7 velocity \u00d7 stealFade    |    Volume Release LFO \u2192 release time',
    style='voice', bx=fi_bx, bw=fi_bw)
y_in += BH + 12*S

frame_bot = y_in + 2*S
d.rounded_rectangle([BX + 8*S, frame_top, BX + BW - 8*S, frame_bot],
                    radius=8*S, fill=None, outline='#4a3888', width=2*S)
fl     = 'SamplerVoice \u00d7 N  (up to 16 voices)'
fl_tw  = tw(fl, F_HDR)
fx     = CX - fl_tw // 2
d.rectangle([fx - 4*S, frame_top - 1*S, fx + fl_tw + 4*S, frame_top + 14*S], fill=BG)
d.text((fx, frame_top + 1*S), fl, fill='#7060b0', font=F_HDR)

y = frame_bot + GAP + 4*S
arr_v(CX, y - GAP, y)

box(y, 'Voice Mix Sum', 'Polyphonic accumulation into stereo buffer', style='engine')
y += BH + GAP + 10*S

# ── PRE-FX ───────────────────────────────────────────────────────────────────
y = hdr(y, 'PRE-FX')
arr_v(CX, y - 2*S, y + 4*S)

box(y, 'Pre-FX VU Meter', 'Peak capture: vuPreFxL  \u00b7  vuPreFxR', style='utility')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Audio Envelope Follower',
    'audioEnvLevel \u2014 dynamic modulation source for Filter, Distortion, Phaser, Delay',
    style='utility')
aef_mid = y + BH // 2
y += BH + GAP + 10*S

# ── FX CHAIN ─────────────────────────────────────────────────────────────────
y = hdr(y, 'FX CHAIN')
arr_v(CX, y - 2*S, y + 4*S)

box(y, 'Stereo Width', style='always')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Limiter', style='always')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Gain  (Smoothed)', '\u221240 to +12 dB  \u00b7  20 ms ramp  \u00b7  juce::SmoothedValue', style='always')
y += BH + GAP + 8*S

dv = y + 6*S
d.line([(BX + 24*S, dv), (BX + BW - 24*S, dv)], fill='#2a2a3a', width=1*S)
ot    = 'optional'
ot_tw = tw(ot, F_TINY)
d.rectangle([CX - ot_tw//2 - 4*S, dv - 7*S, CX + ot_tw//2 + 4*S, dv + 7*S], fill=BG)
d.text((CX - ot_tw//2, dv - 5*S), ot, fill='#404060', font=F_TINY)
y += 16*S
arr_v(CX, y - 2*S, y + 4*S)

box(y, 'Sidechain \u2014 Pre-FX',
    'Division  \u00b7  Pre-Delay  \u00b7  Rise  \u00b7  Curve  \u00b7  Fall  \u00b7  Mix',
    style='optional')
y += BH + GAP
arr_v(CX, y - GAP, y)

filter_mid = y + 31*S
box(y, 'Filter',
    'LP / HP / BP / Comb+ / Comb\u2212',
    'MOD: Gate Env \u00b7 Audio Env \u00b7 Block-rate LFO \u2192 Cutoff  (\u00b14 octaves)',
    style='optional', h=62*S)
y += 62*S + GAP
arr_v(CX, y - GAP, y)

dist_mid = y + 31*S
box(y, 'Distortion',
    'BitCrush \u00b7 HardClip \u00b7 SoftClip \u00b7 Foldback \u00b7 RingMod \u00b7 HardSat \u00b7 SoftSat \u00b7 Waveshaper  \u00b7  3-band (350 Hz / 3.5 kHz)',
    'MOD: Gate Env \u00b7 Audio Env \u2192 Drive',
    style='optional', h=62*S)
y += 62*S + GAP
arr_v(CX, y - GAP, y)

flanger_mid = y + BH // 2
box(y, 'Flanger',
    'Rate \u00b7 Feedback \u00b7 Delay Start/End (1\u201320 ms) \u00b7 Mix    |    MOD: Gate Env \u2192 Mix',
    style='optional')
y += BH + GAP
arr_v(CX, y - GAP, y)

phaser_mid = y + 31*S
box(y, 'Phaser',
    'Rate \u00b7 Depth \u00b7 Feedback \u00b7 Stages (2\u201312) \u00b7 Centre Freq \u00b7 Mix',
    'MOD: Gate Env \u00b7 Audio Env \u2192 Mix',
    style='optional', h=62*S)
y += 62*S + GAP
arr_v(CX, y - GAP, y)

delay_mid = y + 31*S
box(y, 'Ping-Pong Delay',
    'Tempo Sync \u00b7 Feedback \u226499%  \u00b7  FB Filter (LP/HP/BP)  \u00b7  FB Filter LFO  \u00b7  Mix LFO',
    'MOD: Gate Env \u2192 Mix    |    FB Filter LFO \u2192 Cutoff (\u00b12 octaves)',
    style='optional', h=62*S)
y += 62*S + GAP
arr_v(CX, y - GAP, y)

box(y, 'Sidechain \u2014 Post-FX',
    'Division  \u00b7  Pre-Delay  \u00b7  Rise  \u00b7  Curve  \u00b7  Fall  \u00b7  Mix',
    style='optional')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Highcut', 'Low-Pass  \u00b7  200\u201320\u202f000 Hz  \u00b7  1\u20138 stages  (12 dB / stage)', style='optional')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Lowcut', 'High-Pass  \u00b7  20\u20132\u202f000 Hz  \u00b7  1\u20138 stages  (12 dB / stage)', style='optional')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Clip', 'Hard / Soft', style='optional')
y += BH + GAP + 10*S

# ── OUTPUT ───────────────────────────────────────────────────────────────────
y = hdr(y, 'OUTPUT')
arr_v(CX, y - 2*S, y + 4*S)

box(y, 'Post-FX VU Meter', 'Peak capture: vuPostFxL  \u00b7  vuPostFxR', style='utility')
y += BH + GAP
arr_v(CX, y - GAP, y)

box(y, 'Audio Output', style='output')
y += BH

final_y = y + 44*S

# ── Modulation routing lines (right side) ────────────────────────────────────
gc_col  = '#c05050'
aef_col = '#70a030'
gl_x    = BX + BW + 14*S
al_x    = BX + BW + 28*S

d.line([(gl_x, gate_mid), (gl_x, delay_mid)], fill=gc_col, width=1*S)
for ty in [filter_mid, dist_mid, flanger_mid, phaser_mid, delay_mid]:
    d.line([(gl_x, ty), (BX + BW, ty)], fill=gc_col, width=1*S)
    d.polygon([(BX+BW - 5*S, ty - 3*S), (BX+BW - 5*S, ty + 3*S), (BX+BW, ty)], fill=gc_col)

gt    = 'Gate Env'
gt_tw = tw(gt, F_TINY)
d.rectangle([gl_x - 2*S, gate_mid - 14*S, gl_x + gt_tw + 2*S, gate_mid - 2*S], fill=BG)
d.text((gl_x, gate_mid - 13*S), gt, fill=gc_col, font=F_TINY)

d.line([(al_x, aef_mid), (al_x, delay_mid + 6*S)], fill=aef_col, width=1*S)
for ty in [filter_mid + 6*S, dist_mid + 6*S, phaser_mid + 6*S, delay_mid + 6*S]:
    d.line([(al_x, ty), (BX + BW, ty)], fill=aef_col, width=1*S)
    d.polygon([(BX+BW - 5*S, ty - 3*S), (BX+BW - 5*S, ty + 3*S), (BX+BW, ty)], fill=aef_col)

at    = 'Audio Env'
at_tw = tw(at, F_TINY)
d.rectangle([al_x - 2*S, aef_mid - 14*S, al_x + at_tw + 2*S, aef_mid - 2*S], fill=BG)
d.text((al_x, aef_mid - 13*S), at, fill=aef_col, font=F_TINY)

# ── Legend ────────────────────────────────────────────────────────────────────
leg_y = final_y - 34*S
d.line([(BX, leg_y), (BX + BW, leg_y)], fill='#2a2a40', width=1*S)
items = [
    ('#2a9a5a', 'Always active'),
    ('#c07030', 'Optional'),
    ('#2a7ab0', 'MIDI / routing'),
    ('#6a4ab0', 'Voice engine'),
    ('#2a9090', 'Utility / metering'),
    ('#c05050', 'Gate Env mod'),
    ('#70a030', 'Audio Env mod'),
]
lx = BX + 2*S
ly = leg_y + 5*S
for col, label in items:
    d.rectangle([lx, ly + 1*S, lx + 10*S, ly + 9*S], fill=col)
    d.text((lx + 13*S, ly), label, fill='#8090a0', font=F_TINY)
    lx += tw(label, F_TINY) + 26*S

# ── Save ──────────────────────────────────────────────────────────────────────
out  = img.crop((0, 0, W, final_y))
path = r'C:\DevSuomiSamplerVST\signal_path_print.png'
out.save(path, dpi=(300, 300))
print(f'Saved: {path}  ({W} x {final_y} px at 300 DPI)')
