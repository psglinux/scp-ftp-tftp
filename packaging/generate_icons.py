#!/usr/bin/env python3
"""Generate GotiKinesis (GK) application icons and NSIS installer bitmaps."""

from PIL import Image, ImageDraw, ImageFont
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Brand colours
BG_TOP  = (0x0D, 0x47, 0xA1)   # Deep blue
BG_BOT  = (0x15, 0x65, 0xC0)   # Medium blue
ACCENT  = (0x4F, 0xC3, 0xF7)   # Light cyan
WHITE   = (255, 255, 255)
GOLD    = (0xFF, 0xC1, 0x07)   # Amber accent


def rounded_rect(draw, xy, radius, fill):
    x0, y0, x1, y1 = xy
    draw.rectangle([x0 + radius, y0, x1 - radius, y1], fill=fill)
    draw.rectangle([x0, y0 + radius, x1, y1 - radius], fill=fill)
    for cx, cy, s, e in [
        (x0 + radius, y0 + radius, 180, 270),
        (x1 - radius, y0 + radius, 270, 360),
        (x0 + radius, y1 - radius, 90, 180),
        (x1 - radius, y1 - radius, 0, 90),
    ]:
        draw.pieslice([cx - radius, cy - radius, cx + radius, cy + radius],
                      s, e, fill=fill)


def gradient_bg(img, xy, radius, top, bot):
    """Draw a rounded rect with vertical gradient."""
    x0, y0, x1, y1 = xy
    w, h = x1 - x0, y1 - y0
    mask = Image.new("RGBA", img.size, (0, 0, 0, 0))
    md = ImageDraw.Draw(mask)
    rounded_rect(md, xy, radius, (255, 255, 255, 255))

    grad = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gp = grad.load()
    mp = mask.load()
    for py in range(y0, y1):
        t = (py - y0) / max(h - 1, 1)
        r = int(top[0] + (bot[0] - top[0]) * t)
        g = int(top[1] + (bot[1] - top[1]) * t)
        b = int(top[2] + (bot[2] - top[2]) * t)
        for px in range(x0, x1):
            if mp[px, py][3] > 0:
                gp[px, py] = (r, g, b, 255)
    return Image.alpha_composite(img, grad)


def draw_arrow(draw, cx, cy, size, direction, color, width):
    half = size // 2
    head_w = size * 0.55
    head_h = size * 0.32

    if direction == "up":
        draw.rectangle(
            [cx - width // 2, cy - half + head_h, cx + width // 2, cy + half],
            fill=color)
        draw.polygon(
            [(cx, cy - half),
             (cx - head_w / 2, cy - half + head_h),
             (cx + head_w / 2, cy - half + head_h)],
            fill=color)
    else:
        draw.rectangle(
            [cx - width // 2, cy - half, cx + width // 2, cy + half - head_h],
            fill=color)
        draw.polygon(
            [(cx, cy + half),
             (cx - head_w / 2, cy + half - head_h),
             (cx + head_w / 2, cy + half - head_h)],
            fill=color)


def find_font(size):
    """Find a bold font, falling back to default."""
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/arialbd.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def make_gk_icon(size):
    """Compact GK icon: blue rounded square with 'GK' text and small arrows."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    margin = max(size // 16, 1)
    radius = max(size // 5, 2)

    img = gradient_bg(img, (margin, margin, size - margin, size - margin),
                      radius, BG_TOP, BG_BOT)
    draw = ImageDraw.Draw(img)

    body = size - 2 * margin

    if size >= 48:
        # "GK" text in upper portion
        font_sz = int(body * 0.38)
        font = find_font(font_sz)
        bbox = draw.textbbox((0, 0), "GK", font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx = (size - tw) // 2
        ty = margin + int(body * 0.08)
        draw.text((tx, ty), "GK", fill=GOLD, font=font)

        # Smaller arrows below text
        arrow_h = int(body * 0.30)
        arrow_w = max(int(body * 0.08), 2)
        gap = int(body * 0.14)
        arrow_cy = ty + th + int(body * 0.05) + arrow_h // 2

        draw_arrow(draw, size // 2 - gap, arrow_cy, arrow_h, "up", WHITE, arrow_w)
        draw_arrow(draw, size // 2 + gap, arrow_cy, arrow_h, "down", ACCENT, arrow_w)
    else:
        # Very small sizes: just arrows, no text
        arrow_h = int(body * 0.50)
        arrow_w = max(int(body * 0.10), 2)
        gap = int(body * 0.16)
        draw_arrow(draw, size // 2 - gap, size // 2, arrow_h, "up", WHITE, arrow_w)
        draw_arrow(draw, size // 2 + gap, size // 2, arrow_h, "down", ACCENT, arrow_w)

    return img


def make_full_logo(width, height):
    """Full 'GotiKinesis' logo: icon on left, text on right, dark bg."""
    bg_color = (0x1A, 0x23, 0x2F)
    img = Image.new("RGB", (width, height), bg_color)
    draw = ImageDraw.Draw(img)

    icon_sz = min(height - 16, width // 3)
    icon = make_gk_icon(icon_sz)
    iy = (height - icon_sz) // 2
    # Composite icon onto dark background
    tmp = Image.new("RGBA", (width, height), (*bg_color, 255))
    tmp.paste(icon, (8, iy), icon)
    img = tmp.convert("RGB")
    draw = ImageDraw.Draw(img)

    text_x = icon_sz + 24
    available_w = width - text_x - 12

    for fsz in range(height // 2, 8, -1):
        font = find_font(fsz)
        bbox = draw.textbbox((0, 0), "GotiKinesis", font=font)
        if bbox[2] - bbox[0] <= available_w:
            break

    bbox = draw.textbbox((0, 0), "GotiKinesis", font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    ty = (height - th) // 2 - int(height * 0.12)
    draw.text((text_x, ty), "Goti", fill=GOLD, font=font)
    goti_w = draw.textbbox((0, 0), "Goti", font=font)[2]
    draw.text((text_x + goti_w, ty), "Kinesis", fill=WHITE, font=font)

    sub_fsz = max(fsz // 3, 10)
    sub_font = find_font(sub_fsz)
    sub_text = "SCP  /  FTP  /  TFTP  File Transfer"
    draw.text((text_x, ty + th + 6), sub_text, fill=ACCENT, font=sub_font)

    return img


def make_ico(path, sizes=(16, 24, 32, 48, 64, 128, 256)):
    largest = make_gk_icon(max(sizes))
    largest.save(path, format="ICO", sizes=[(s, s) for s in sizes])
    print(f"  {path}")


def make_png(path, size=256):
    img = make_gk_icon(size)
    img.save(path, format="PNG")
    print(f"  {path}")


def make_logo_png(path, w=600, h=160):
    img = make_full_logo(w, h)
    img.save(path, format="PNG")
    print(f"  {path}")


def make_nsis_header(path, w=150, h=57):
    img = Image.new("RGB", (w, h), (255, 255, 255))
    icon = make_gk_icon(h - 8).convert("RGB")
    img.paste(icon, (w - h + 2, 4))
    img.save(path, format="BMP")
    print(f"  {path}")


def make_nsis_welcome(path, w=164, h=314):
    img = Image.new("RGB", (w, h), (255, 255, 255))
    draw = ImageDraw.Draw(img)

    # Gradient strip at bottom
    strip_h = 120
    for y in range(strip_h):
        t = y / strip_h
        r = int(255 + (BG_TOP[0] - 255) * t)
        g = int(255 + (BG_TOP[1] - 255) * t)
        b = int(255 + (BG_TOP[2] - 255) * t)
        draw.line([(0, h - strip_h + y), (w, h - strip_h + y)], fill=(r, g, b))

    # Icon centred in upper portion
    icon_sz = min(w - 20, 120)
    icon = make_gk_icon(icon_sz).convert("RGB")
    img.paste(icon, ((w - icon_sz) // 2, 40))

    # "GotiKinesis" text below icon
    font = find_font(16)
    bbox = draw.textbbox((0, 0), "GotiKinesis", font=font)
    tw = bbox[2] - bbox[0]
    tx = (w - tw) // 2
    ty = 40 + icon_sz + 12
    draw.text((tx, ty), "GotiKinesis", fill=BG_TOP, font=font)

    # "GK" subtitle
    sub_font = find_font(11)
    sub = "SCP / FTP / TFTP"
    bbox2 = draw.textbbox((0, 0), sub, font=sub_font)
    draw.text(((w - bbox2[2] + bbox2[0]) // 2, ty + 22), sub,
              fill=(0x60, 0x60, 0x60), font=sub_font)

    img.save(path, format="BMP")
    print(f"  {path}")


if __name__ == "__main__":
    win_dir = os.path.join(SCRIPT_DIR, "windows")
    linux_dir = os.path.join(SCRIPT_DIR, "linux")
    os.makedirs(win_dir, exist_ok=True)
    os.makedirs(linux_dir, exist_ok=True)

    print("Generating GotiKinesis icons and bitmaps:")

    # Windows
    make_ico(os.path.join(win_dir, "app.ico"))
    make_png(os.path.join(win_dir, "app.png"), 256)
    make_nsis_header(os.path.join(win_dir, "nsis_header.bmp"))
    make_nsis_welcome(os.path.join(win_dir, "nsis_welcome.bmp"))

    # Linux
    for sz in (48, 128, 256):
        make_png(os.path.join(linux_dir, f"gotikinesis_{sz}.png"), sz)

    # Full logo for about dialog / splash
    make_logo_png(os.path.join(SCRIPT_DIR, "logo.png"), 600, 160)

    print("Done.")
