from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_BLACK = ROOT / "tools" / "fonts" / "Orbitron Black.ttf"
FONT_BOLD = ROOT / "tools" / "fonts" / "Orbitron Bold.ttf"
PREVIEW_OUT = ROOT / "tools" / "preview_orbitron_clock_variants.png"

CHARS = "0123456789:"
LINES = ["0123456789", "12:48", "20:31", "09:05"]
TARGET_HEIGHTS = [26, 28, 30]
THRESHOLDS = [80, 96, 112, 128]
MAX_GLYPH_WIDTH = 24
GAP = 2


def find_font_path() -> Path:
    if FONT_BLACK.exists():
        return FONT_BLACK
    if FONT_BOLD.exists():
        return FONT_BOLD
    raise FileNotFoundError("Orbitron Black.ttf or Orbitron Bold.ttf not found under tools/fonts")


def render_char(font: ImageFont.FreeTypeFont, char: str, threshold: int) -> Image.Image:
    bbox = font.getbbox(char)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    canvas = Image.new("L", (width + 8, height + 8), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((4 - bbox[0], 4 - bbox[1]), char, font=font, fill=255)
    bbox = canvas.getbbox()
    if bbox is None:
        return Image.new("1", (1, 1), 0)

    glyph = canvas.crop(bbox)
    return glyph.point(lambda p: 255 if p >= threshold else 0, mode="1")


def load_font_for_height(font_path: Path, target_height: int, threshold: int) -> ImageFont.FreeTypeFont:
    best_font = None
    best_score = 10000
    for size in range(18, 72):
        font = ImageFont.truetype(str(font_path), size=size)
        glyphs = {char: render_char(font, char, threshold) for char in CHARS}
        digit_width = max(glyphs[char].width for char in "0123456789")
        digit_height = max(glyphs[char].height for char in "0123456789")
        if digit_width > MAX_GLYPH_WIDTH or digit_height > target_height:
            continue

        score = (target_height - digit_height) * 10 + (MAX_GLYPH_WIDTH - digit_width)
        if score < best_score:
            best_score = score
            best_font = font

    if best_font is None:
        raise ValueError(f"No Orbitron size fits target_height={target_height}, threshold={threshold}")
    return best_font


def rows_for(glyph: Image.Image, fixed_width: int, target_height: int):
    x_offset = (fixed_width - glyph.width) // 2
    y_offset = (target_height - glyph.height) // 2
    rows = []
    for y in range(target_height):
        bits = 0
        for x in range(fixed_width):
            source_x = x - x_offset
            source_y = y - y_offset
            on = 0 <= source_x < glyph.width and 0 <= source_y < glyph.height and glyph.getpixel((source_x, source_y)) != 0
            if on:
                bits |= 1 << (fixed_width - 1 - x)
        rows.append(bits)
    return rows


def make_variant(font_path: Path, target_height: int, threshold: int):
    font = load_font_for_height(font_path, target_height, threshold)
    glyphs = {char: render_char(font, char, threshold) for char in CHARS}
    digit_width = max(glyphs[char].width for char in "0123456789")
    colon_width = glyphs[":"].width
    glyph_widths = {char: digit_width for char in "0123456789"}
    glyph_widths[":"] = colon_width
    rows = {char: rows_for(glyphs[char], glyph_widths[char], target_height) for char in CHARS}
    return rows, glyph_widths


def text_width(text: str, glyph_widths, x_num: int, x_den: int) -> int:
    width = sum(glyph_widths[char] for char in text) + (len(text) - 1) * GAP
    return (width * x_num + x_den - 1) // x_den


def draw_text(draw: ImageDraw.ImageDraw, rows, glyph_widths, text: str, x: int, y: int, x_num: int, x_den: int, y_scale: int, color):
    cursor = 0
    for char in text:
        glyph_rows = rows[char]
        glyph_width = glyph_widths[char]
        cursor_x = x + (cursor * x_num) // x_den
        for row_index, bits in enumerate(glyph_rows):
            col = 0
            while col < glyph_width:
                while col < glyph_width and (bits & (1 << (glyph_width - 1 - col))) == 0:
                    col += 1
                start = col
                while col < glyph_width and (bits & (1 << (glyph_width - 1 - col))) != 0:
                    col += 1
                if col > start:
                    x0 = cursor_x + (start * x_num) // x_den
                    x1 = cursor_x + (col * x_num + x_den - 1) // x_den
                    y0 = y + row_index * y_scale
                    draw.rectangle([x0, y0, x1 - 1, y0 + y_scale - 1], fill=color)
        cursor += glyph_width + GAP


def draw_variant_card(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, title: str, rows, glyph_widths, target_height: int):
    bg = (8, 13, 18)
    border = (32, 48, 62)
    text = (238, 242, 246)
    muted = (134, 166, 190)
    orange = (255, 126, 34)

    draw.rounded_rectangle([x, y, x + w, y + h], radius=8, fill=bg, outline=border)
    draw.text((x + 12, y + 10), title, fill=orange)

    sample_y = y + 38
    for line in LINES:
        scale = 2 if line != "0123456789" else 1
        sample_width = text_width(line, glyph_widths, scale, 1)
        draw_text(draw, rows, glyph_widths, line, x + 12 + (w - 24 - sample_width) // 2, sample_y, scale, 1, scale, text)
        sample_y += target_height * scale + 12

    monitor_line = "20:31"
    monitor_width = text_width(monitor_line, glyph_widths, 4, 3)
    monitor_y = y + h - target_height * 2 - 26
    draw.text((x + 12, monitor_y - 16), "pc monitor 4/3 x 2", fill=muted)
    draw_text(draw, rows, glyph_widths, monitor_line, x + 12 + (w - 24 - monitor_width) // 2, monitor_y, 4, 3, 2, text)


def main():
    font_path = find_font_path()
    card_w = 360
    card_h = 390
    gap = 18
    margin = 20
    label_h = 34

    image_w = margin * 2 + len(THRESHOLDS) * card_w + (len(THRESHOLDS) - 1) * gap
    image_h = margin * 2 + label_h + len(TARGET_HEIGHTS) * card_h + (len(TARGET_HEIGHTS) - 1) * gap
    image = Image.new("RGB", (image_w, image_h), (4, 7, 10))
    draw = ImageDraw.Draw(image)
    draw.text((margin, margin), f"Orbitron clock bitmap variants - {font_path.name}", fill=(238, 242, 246))

    for row, target_height in enumerate(TARGET_HEIGHTS):
        for col, threshold in enumerate(THRESHOLDS):
            rows, glyph_widths = make_variant(font_path, target_height, threshold)
            x = margin + col * (card_w + gap)
            y = margin + label_h + row * (card_h + gap)
            title = f"h={target_height} threshold={threshold} digit={glyph_widths['0']} colon={glyph_widths[':']}"
            draw_variant_card(draw, x, y, card_w, card_h, title, rows, glyph_widths, target_height)

    image.save(PREVIEW_OUT)
    print(f"Generated {PREVIEW_OUT}")


if __name__ == "__main__":
    main()
