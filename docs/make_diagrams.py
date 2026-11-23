"""Generates docs/schematic.svg and docs/wiring.svg.

Run:  python docs/make_diagrams.py
Pin assignment follows config.h (ESP32 block, ESP8266 values in the schematic).
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent
FONT = "Segoe UI, Helvetica, Arial, sans-serif"
MONO = "Consolas, Menlo, monospace"

INK = "#1f2937"
MUTED = "#6b7280"
BOARD = "#f3f4f6"
PCB = "#14532d"
PCB_DARK = "#0f3d22"


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.parts = []

    def add(self, s):
        self.parts.append(s)

    def rect(self, x, y, w, h, fill="none", stroke=INK, sw=1.5, rx=6, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def line(self, x1, y1, x2, y2, stroke=INK, sw=1.5, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def poly(self, pts, stroke=INK, sw=1.5):
        p = " ".join(f"{x},{y}" for x, y in pts)
        self.add(f'<polyline points="{p}" fill="none" stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round" stroke-linecap="round"/>')

    def text(self, x, y, s, size=13, fill=INK, anchor="start", weight="normal", mono=False, italic=False):
        fam = MONO if mono else FONT
        st = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}"{st}>{esc(s)}</text>')

    def dot(self, x, y, r=4, fill=INK):
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"/>')

    def save(self, name, title):
        body = "\n".join(self.parts)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
               f'viewBox="0 0 {self.w} {self.h}" role="img" aria-label="{esc(title)}">\n'
               f'<title>{esc(title)}</title>\n'
               f'<rect width="100%" height="100%" fill="#ffffff"/>\n{body}\n</svg>\n')
        (OUT / name).write_text(svg, encoding="utf-8")
        print("wrote", OUT / name)


# ---------------------------------------------------------------------------
# Schematic (net labels)
# ---------------------------------------------------------------------------

NET_COLORS = {
    "+5V": "#dc2626", "+3V3": "#ea580c", "GND": "#111827",
    "STB1": "#ca8a04", "STB2": "#a16207", "CLK": "#2563eb", "DIN": "#16a34a",
    "STB3": "#c026d3", "CLK3": "#0891b2", "DIN3": "#65a30d",
    "SDA": "#0d9488", "SCL": "#7c3aed",
}


def net_label(svg, x, y, net, side="right"):
    """Flag-shaped net label; the wire ends at (x, y)."""
    c = NET_COLORS[net]
    w = 14 + 8.2 * len(net)
    h = 20
    if side == "right":   # flag extends to the right of x
        pts = [(x, y), (x + 9, y - h / 2), (x + w, y - h / 2), (x + w, y + h / 2), (x + 9, y + h / 2)]
        tx = x + 9 + (w - 9) / 2
    else:
        pts = [(x, y), (x - 9, y - h / 2), (x - w, y - h / 2), (x - w, y + h / 2), (x - 9, y + h / 2)]
        tx = x - 9 - (w - 9) / 2
    p = " ".join(f"{a},{b}" for a, b in pts)
    svg.add(f'<polygon points="{p}" fill="#ffffff" stroke="{c}" stroke-width="1.6"/>')
    svg.text(tx, y + 4.5, net, size=12, fill=c, anchor="middle", weight="bold", mono=True)


def pin_right(svg, bx, y, name, net, sub=None, stub=46):
    """Pin on the right edge of a box at x=bx."""
    c = NET_COLORS[net]
    svg.text(bx - 10, y + 4.5, name, size=13, anchor="end", mono=True, weight="bold")
    if sub:
        svg.text(bx - 10, y + 18, sub, size=10.5, fill=MUTED, anchor="end", mono=True)
    svg.line(bx, y, bx + stub, y, stroke=c, sw=2)
    svg.dot(bx, y, 2.5, c)
    net_label(svg, bx + stub, y, net, "right")


def pin_left(svg, bx, y, name, net, num=None, stub=46):
    c = NET_COLORS[net]
    label = f"{num} {name}" if num is not None else name
    svg.text(bx + 10, y + 4.5, label, size=13, mono=True, weight="bold")
    svg.line(bx - stub, y, bx, y, stroke=c, sw=2)
    svg.dot(bx, y, 2.5, c)
    net_label(svg, bx - stub, y, net, "left")


def schematic():
    s = Svg(1400, 980)
    s.text(40, 48, "AwqatSalahEsp8266 – schematic", size=24, weight="bold")
    s.text(40, 74, "Connections are made by net name: pins with the same label are wired together. "
           "Pin numbers follow config.h.", size=13, fill=MUTED)

    # ---- MCU -------------------------------------------------------------
    mx, my, mw, mh = 330, 120, 300, 700
    s.rect(mx, my, mw, mh, fill=BOARD, sw=2, rx=10)
    s.text(mx + mw / 2, my + 30, "U1  ESP32 DevKit", size=16, anchor="middle", weight="bold")
    s.text(mx + mw / 2, my + 50, "(grey: ESP8266 NodeMCU / D1 mini)", size=11.5, fill=MUTED, anchor="middle")

    sig = [
        ("IO19", "STB1", "GPIO12 / D6"),
        ("IO17", "STB2", "GPIO14 / D5"),
        ("IO18", "CLK", "GPIO13 / D7"),
        ("IO5", "DIN", "GPIO0 / D3"),
        ("IO4", "STB3", "GPIO15 / D8"),
        ("IO0", "CLK3", "= CLK (GPIO13)"),
        ("IO2", "DIN3", "= DIN (GPIO0)"),
        ("IO22", "SDA", "GPIO4 / D2"),
        ("IO21", "SCL", "GPIO5 / D1"),
    ]
    y0 = my + 100
    for i, (name, net, sub) in enumerate(sig):
        pin_right(s, mx + mw, y0 + i * 62, name, net, sub)

    pw = [("5V / VIN", "+5V"), ("3V3", "+3V3"), ("GND", "GND")]
    for i, (name, net) in enumerate(pw):
        pin_left(s, mx, y0 + 40 + i * 80, name, net)

    # ---- power supply -----------------------------------------------------
    s.rect(40, 520, 200, 120, fill=BOARD, sw=2, rx=10)
    s.text(140, 552, "PS1  5 V supply", size=15, anchor="middle", weight="bold")
    s.text(140, 572, "clock's own supply", size=11.5, fill=MUTED, anchor="middle")
    s.text(140, 588, "or USB adapter ≥ 1 A", size=11.5, fill=MUTED, anchor="middle")
    # outputs
    s.line(240, 600, 262, 600, stroke=NET_COLORS["+5V"], sw=2)
    net_label(s, 262, 600, "+5V")
    s.line(240, 626, 262, 626, stroke=NET_COLORS["GND"], sw=2)
    net_label(s, 262, 626, "GND")
    s.text(232, 604, "+", size=13, anchor="end", weight="bold", mono=True)
    s.text(232, 630, "−", size=13, anchor="end", weight="bold", mono=True)

    # ---- display board 1 (prayer times) -----------------------------------
    bx, by, bw, bh = 940, 110, 420, 330
    s.rect(bx, by, bw, bh, fill="#fef2f2", stroke="#991b1b", sw=2, rx=10)
    s.text(bx + bw / 2, by + 28, "Eratec display board – prayer times", size=15, anchor="middle", weight="bold", fill="#7f1d1d")
    s.text(bx + 12, by + 52, "J1  6-pin connector", size=12, fill=MUTED)
    j1 = [("GND", "GND"), ("VDD", "+5V"), ("STB", "STB1"), ("SCK", "CLK"), ("DIN", "DIN"), ("STB2", "STB2")]
    for i, (name, net) in enumerate(j1):
        pin_left(s, bx, by + 80 + i * 38, name, net, num=i + 1)
    # chips
    cx = bx + 200
    for k, (title, sub, stb_row) in enumerate([
        ("U2  TA6932 #1", "digits 0–15", 2),
        ("U3  TA6932 #2", "digits 16–31", 5),
    ]):
        cy = by + 70 + k * 130
        s.rect(cx, cy, 190, 100, fill="#ffffff", stroke="#991b1b", sw=1.5)
        s.text(cx + 95, cy + 26, title, size=13.5, anchor="middle", weight="bold")
        s.text(cx + 95, cy + 46, sub, size=12, anchor="middle", fill=MUTED)
        s.text(cx + 12, cy + 70, "STB  CLK  DIN", size=11, mono=True, fill=MUTED)
        s.text(cx + 12, cy + 88, "VDD  GND", size=11, mono=True, fill=MUTED)
    s.text(bx + bw - 14, by + bh - 14, "CLK, DIN, VDD, GND shared by U2/U3", size=11, fill=MUTED, anchor="end", italic=True)
    # internal strobe hints
    s.line(bx + 80, by + 156, cx, by + 120, stroke=NET_COLORS["STB1"], sw=1.2, dash="4 3")
    s.line(bx + 86, by + 270, cx, by + 250, stroke=NET_COLORS["STB2"], sw=1.2, dash="4 3")

    # ---- clock / date board ----------------------------------------------
    bx2, by2, bh2 = 940, 480, 250
    s.rect(bx2, by2, bw, bh2, fill="#eff6ff", stroke="#1e3a8a", sw=2, rx=10)
    s.text(bx2 + bw / 2, by2 + 28, "Eratec clock / date board", size=15, anchor="middle", weight="bold", fill="#1e3a8a")
    s.text(bx2 + 12, by2 + 52, "J2  (pin order: check your board)", size=12, fill=MUTED)
    j2 = [("STB", "STB3"), ("CLK", "CLK3"), ("DIN", "DIN3"), ("VDD", "+5V"), ("GND", "GND")]
    for i, (name, net) in enumerate(j2):
        pin_left(s, bx2, by2 + 80 + i * 36, name, net)
    s.rect(cx, by2 + 70, 190, 100, fill="#ffffff", stroke="#1e3a8a", sw=1.5)
    s.text(cx + 95, by2 + 96, "U4  TA6932 #3", size=13.5, anchor="middle", weight="bold")
    s.text(cx + 95, by2 + 116, "digits 32–47", size=12, anchor="middle", fill=MUTED)
    s.text(cx + 95, by2 + 136, "date, time", size=12, anchor="middle", fill=MUTED)
    s.text(bx2 + bw - 14, by2 + bh2 - 14, "only with DISPLAY_CHIPS 3", size=11, fill=MUTED, anchor="end", italic=True)

    # ---- sensors ----------------------------------------------------------
    sy = 770
    s.rect(940, sy, 200, 180, fill="#f0fdfa", stroke="#115e59", sw=2, rx=10)
    s.text(1040, sy + 26, "U5  BME280", size=14, anchor="middle", weight="bold", fill="#134e4a")
    s.text(1040, sy + 44, "addr 0x76", size=11, anchor="middle", fill=MUTED)
    for i, (name, net) in enumerate([("VCC", "+3V3"), ("GND", "GND"), ("SCL", "SCL"), ("SDA", "SDA")]):
        pin_left(s, 940, sy + 70 + i * 28, name, net, stub=40)

    s.rect(1160, sy, 200, 180, fill="#ffffff", stroke="#6b7280", sw=1.8, rx=10, dash="6 4")
    s.text(1260, sy + 26, "U6  DS3231 RTC", size=14, anchor="middle", weight="bold", fill=MUTED)
    s.text(1260, sy + 44, "optional, addr 0x68", size=11, anchor="middle", fill=MUTED)
    for i, (name, net) in enumerate([("VCC", "+3V3"), ("GND", "GND"), ("SCL", "SCL"), ("SDA", "SDA")]):
        s.text(1180, sy + 74 + i * 28, f"{name}", size=13, mono=True, weight="bold", fill=MUTED)
        s.text(1225, sy + 74 + i * 28, f"→ {net}", size=12, mono=True, fill=NET_COLORS[net])

    # ---- notes ------------------------------------------------------------
    notes = [
        "Notes",
        "• Display boards run at 5 V, the ESP drives them with 3.3 V logic.",
        "  Works in practice; if digits flicker, add a 74AHCT125 level shifter.",
        "• Strapping pins: ESP32 IO0, IO2, IO5 and ESP8266 GPIO0, GPIO15",
        "  must keep their default level during power-up.",
        "• BME280 and DS3231 share the I2C bus (modules have pull-ups).",
        "• Common GND for all parts.",
    ]
    ny = 860
    for i, n in enumerate(notes):
        s.text(40, ny + i * 18, n, size=12.5 if i else 13.5, fill=INK if i == 0 else MUTED,
               weight="bold" if i == 0 else "normal")

    s.save("schematic.svg", "AwqatSalahEsp8266 schematic")


# ---------------------------------------------------------------------------
# Wiring diagram (ESP32-DevKitC, 38 pins)
# ---------------------------------------------------------------------------

RIGHT_HEADER = ["GND", "IO23", "IO22", "TX", "RX", "IO21", "GND", "IO19", "IO18", "IO5",
                "IO17", "IO16", "IO4", "IO0", "IO2", "IO15", "SD1", "SD0", "CLK"]
LEFT_HEADER = ["3V3", "EN", "VP", "VN", "IO34", "IO35", "IO32", "IO33", "IO25", "IO26",
               "IO27", "IO14", "IO12", "GND", "IO13", "SD2", "SD3", "CMD", "5V"]

WIRE = {
    "5V": "#dc2626", "3V3": "#f97316", "GND": "#111827",
    "STB1": "#eab308", "STB2": "#a16207", "CLK": "#2563eb", "DIN": "#16a34a",
    "STB3": "#d946ef", "CLK3": "#06b6d4", "DIN3": "#84cc16",
    "SDA": "#0d9488", "SCL": "#7c3aed",
}


def wiring():
    s = Svg(1400, 1020)
    s.text(40, 44, "AwqatSalahEsp8266 – wiring (ESP32)", size=24, weight="bold")
    s.text(40, 68, "ESP32-DevKitC (38 pin). On other boards go by the GPIO label, not the position.",
           size=13, fill=MUTED)

    # ---- ESP32 board ------------------------------------------------------
    ex, ey, ew = 400, 170, 190
    pitch = 30
    first = ey + 70
    eh = 70 + pitch * 18 + 50
    s.rect(ex, ey, ew, eh, fill="#1f2937", stroke="#111827", sw=2, rx=8)
    s.rect(ex + 50, ey - 26, 90, 40, fill="#d1d5db", stroke="#6b7280", sw=1.5, rx=3)          # antenna/module
    s.rect(ex + 40, ey + 10, 110, 46, fill="#9ca3af", stroke="#6b7280", sw=1.5, rx=3)         # shield
    s.text(ex + ew / 2, ey + 38, "ESP32-WROOM", size=11.5, anchor="middle", fill="#111827", weight="bold")
    s.rect(ex + 70, ey + eh - 12, 50, 26, fill="#9ca3af", stroke="#6b7280", sw=1.5, rx=3)     # USB
    s.text(ex + ew / 2, ey + eh + 30, "USB", size=11, anchor="middle", fill=MUTED)

    pad_l, pad_r = ex + 14, ex + ew - 14
    pins = {}
    for i, name in enumerate(LEFT_HEADER):
        y = first + i * pitch
        s.dot(pad_l, y, 6, "#d4a017")
        s.dot(pad_l, y, 2.5, "#1f2937")
        s.text(pad_l + 14, y + 4, name, size=11, fill="#e5e7eb", mono=True)
        pins[("L", name, i)] = (pad_l, y)
    for i, name in enumerate(RIGHT_HEADER):
        y = first + i * pitch
        s.dot(pad_r, y, 6, "#d4a017")
        s.dot(pad_r, y, 2.5, "#1f2937")
        s.text(pad_r - 14, y + 4, name, size=11, fill="#e5e7eb", mono=True, anchor="end")
        pins[("R", name, i)] = (pad_r, y)

    def rp(name, nth=0):
        hits = [v for k, v in pins.items() if k[0] == "R" and k[1] == name]
        return hits[nth]

    def lp(name):
        return [v for k, v in pins.items() if k[0] == "L" and k[1] == name][0]

    # ---- targets ----------------------------------------------------------
    tx = 1040   # connector pin x

    def connector(x, y, names, title, sub, fill, stroke, pitch_c=30, w=300):
        h = 60 + pitch_c * len(names)
        s.rect(x - 16, y, w, h, fill=fill, stroke=stroke, sw=2, rx=10)
        s.text(x + 24, y + 26, title, size=14.5, weight="bold", fill=stroke)
        s.text(x + 24, y + 44, sub, size=11.5, fill=MUTED)
        pos = {}
        for i, n in enumerate(names):
            yy = y + 66 + i * pitch_c
            s.rect(x - 7, yy - 7, 14, 14, fill="#ffffff", stroke=stroke, sw=1.5, rx=2)
            s.text(x + 16, yy + 4.5, n, size=12.5, mono=True, weight="bold")
            pos[n] = (x - 7, yy)
        return pos

    bme = connector(tx, 90, ["SDA", "SCL", "GND", "VCC"], "BME280 module", "temperature / humidity, 3.3 V",
                    "#f0fdfa", "#115e59", pitch_c=26, w=300)
    j1 = connector(tx, 300, ["GND", "VDD", "STB", "SCK", "DIN", "STB2"], "Display board – J1",
                   "6-pin ribbon cable, prayer times (2× TA6932)", "#fef2f2", "#991b1b")
    j2 = connector(tx, 610, ["STB", "CLK", "DIN", "VDD", "GND"], "Clock / date board",
                   "3rd TA6932, pin order may differ", "#eff6ff", "#1e3a8a")

    def route(src, lane, dst, color, sw=3.2):
        (x1, y1), (x2, y2) = src, dst
        s.poly([(x1, y1), (lane, y1), (lane, y2), (x2, y2)], stroke="#ffffff", sw=sw + 2.5)
        s.poly([(x1, y1), (lane, y1), (lane, y2), (x2, y2)], stroke=color, sw=sw)

    # signals: lanes chosen so the signal wires never cross
    route(rp("IO22"), 640, bme["SDA"], WIRE["SDA"])
    route(rp("IO21"), 670, bme["SCL"], WIRE["SCL"])
    route(rp("IO19"), 710, j1["STB"], WIRE["STB1"])
    route(rp("IO18"), 740, j1["SCK"], WIRE["CLK"])
    route(rp("IO5"), 770, j1["DIN"], WIRE["DIN"])
    route(rp("IO17"), 800, j1["STB2"], WIRE["STB2"])
    route(rp("IO4"), 840, j2["STB"], WIRE["STB3"])
    route(rp("IO0"), 870, j2["CLK"], WIRE["CLK3"])
    route(rp("IO2"), 900, j2["DIN"], WIRE["DIN3"])

    # ---- power supply -----------------------------------------------------
    px, py = 60, 120
    s.rect(px, py, 200, 110, fill=BOARD, stroke=INK, sw=2, rx=10)
    s.text(px + 100, py + 32, "5 V supply", size=15, weight="bold", anchor="middle")
    s.text(px + 100, py + 52, "≥ 1 A", size=11.5, fill=MUTED, anchor="middle")
    p5 = (px + 200, py + 72)
    pg = (px + 200, py + 94)
    s.text(p5[0] - 10, p5[1] + 4.5, "+5V", size=12, mono=True, anchor="end", weight="bold", fill=WIRE["5V"])
    s.text(pg[0] - 10, pg[1] + 4.5, "GND", size=12, mono=True, anchor="end", weight="bold")

    v5_lane, gnd_lane, v33_lane = 990, 1010, 960
    top5, topg = 100, 112

    # 5V: supply -> bus on top -> J1 VDD, J2 VDD ; branch down the left -> ESP 5V
    s.poly([p5, (300, p5[1]), (300, top5), (v5_lane, top5), (v5_lane, j2["VDD"][1])], stroke=WIRE["5V"], sw=3.2)
    for t in (j1["VDD"], j2["VDD"]):
        s.poly([(v5_lane, t[1]), t], stroke=WIRE["5V"], sw=3.2)
        s.dot(v5_lane, t[1], 4.5, WIRE["5V"])
    esp5 = lp("5V")
    s.poly([(300, p5[1]), (300, esp5[1]), esp5], stroke=WIRE["5V"], sw=3.2)
    s.dot(300, p5[1], 4.5, WIRE["5V"])

    # GND
    s.poly([pg, (330, pg[1]), (330, topg), (gnd_lane, topg), (gnd_lane, j2["GND"][1])], stroke=WIRE["GND"], sw=3.2)
    for t in (bme["GND"], j1["GND"], j2["GND"]):
        s.poly([(gnd_lane, t[1]), t], stroke=WIRE["GND"], sw=3.2)
        s.dot(gnd_lane, t[1], 4.5, WIRE["GND"])
    espg = lp("GND")
    s.poly([(330, pg[1]), (330, espg[1]), espg], stroke=WIRE["GND"], sw=3.2)
    s.dot(330, pg[1], 4.5, WIRE["GND"])

    # 3V3 from the ESP to the BME280
    e33 = lp("3V3")
    s.poly([e33, (360, e33[1]), (360, 82), (v33_lane, 82), (v33_lane, bme["VCC"][1]), bme["VCC"]],
           stroke=WIRE["3V3"], sw=3.2)

    # ---- legend -----------------------------------------------------------
    lx, ly = 40, 880
    s.text(lx, ly, "Wires", size=14, weight="bold")
    legend = [
        ("5V", "+5 V"), ("3V3", "+3.3 V (BME280 only)"), ("GND", "GND"),
        ("STB1", "IO19 → STB"), ("STB2", "IO17 → STB2"), ("CLK", "IO18 → SCK"), ("DIN", "IO5 → DIN"),
        ("STB3", "IO4 → clock STB"), ("CLK3", "IO0 → clock CLK"), ("DIN3", "IO2 → clock DIN"),
        ("SDA", "IO22 → SDA"), ("SCL", "IO21 → SCL"),
    ]
    for i, (k, label) in enumerate(legend):
        col, row = divmod(i, 3)
        x = lx + col * 250
        y = ly + 22 + row * 22
        s.line(x, y - 4, x + 26, y - 4, stroke=WIRE[k], sw=4)
        s.text(x + 34, y, label, size=12)
    s.text(lx, ly + 96, "● = connection, crossing wires without a dot are not connected.", size=11.5, fill=MUTED)
    s.text(lx, ly + 114, "Optional DS3231: same 4 wires as the BME280 (3V3, GND, SDA, SCL).", size=11.5, fill=MUTED)

    s.save("wiring.svg", "AwqatSalahEsp8266 wiring diagram")


if __name__ == "__main__":
    schematic()
    wiring()
