from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_BLACK = ROOT / "tools" / "fonts" / "Orbitron Black.ttf"
FONT_BOLD = ROOT / "tools" / "fonts" / "Orbitron Bold.ttf"
HEADER_OUT = ROOT / "src" / "numeric_clock_font.h"
CPP_OUT = ROOT / "src" / "numeric_clock_font.cpp"
PREVIEW_OUT = ROOT / "tools" / "preview_orbitron_clock_font.png"

CHARS = "0123456789:"
TARGET_HEIGHT = 26
MAX_GLYPH_WIDTH = 24
GAP = 2


def find_font_path() -> Path:
    if FONT_BLACK.exists():
        return FONT_BLACK
    if FONT_BOLD.exists():
        return FONT_BOLD
    raise FileNotFoundError("Orbitron Black.ttf or Orbitron Bold.ttf not found under tools/fonts")


def load_font_for_height(font_path: Path, target_height: int) -> ImageFont.FreeTypeFont:
    best_font = None
    best_score = 10_000
    for size in range(18, 64):
        font = ImageFont.truetype(str(font_path), size=size)
        glyphs = {char: render_char(font, char) for char in CHARS}
        digit_width = max(glyphs[char].width for char in "0123456789")
        digit_height = max(glyphs[char].height for char in "0123456789")
        if digit_width > MAX_GLYPH_WIDTH or digit_height > target_height:
            continue
        score = (target_height - digit_height) * 10 + (MAX_GLYPH_WIDTH - digit_width)
        if score < best_score:
            best_score = score
            best_font = font
    if best_font is None:
        raise ValueError("No Orbitron size fits the logical bitmap constraints")
    return best_font


def render_char(font: ImageFont.FreeTypeFont, char: str):
    bbox = font.getbbox(char)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    canvas = Image.new("L", (w + 8, h + 8), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((4 - bbox[0], 4 - bbox[1]), char, font=font, fill=255)
    bbox = canvas.getbbox()
    if bbox is None:
        return Image.new("1", (1, TARGET_HEIGHT), 0)

    glyph = canvas.crop(bbox)
    thresholded = glyph.point(lambda p: 255 if p >= 96 else 0, mode="1")
    return thresholded


def rows_for(glyph: Image.Image, fixed_width: int):
    x_offset = (fixed_width - glyph.width) // 2
    y_offset = (TARGET_HEIGHT - glyph.height) // 2
    rows = []
    for y in range(TARGET_HEIGHT):
        bits = 0
        for x in range(fixed_width):
            source_x = x - x_offset
            source_y = y - y_offset
            on = 0 <= source_x < glyph.width and 0 <= source_y < glyph.height and glyph.getpixel((source_x, source_y)) != 0
            if on:
                bits |= 1 << (fixed_width - 1 - x)
        rows.append(bits)
    return rows


def c_identifier(char: str) -> str:
    return "COLON" if char == ":" else char


def render_preview(font_rows, glyph_widths):
    lines = ["0123456789", "20:31", "12:48", "09:05"]
    scale = 4
    line_gap = 18
    width = max(sum(glyph_widths[char] for char in line) + (len(line) - 1) * GAP for line in lines)
    height = len(lines) * TARGET_HEIGHT + (len(lines) - 1) * line_gap
    image = Image.new("RGB", (width * scale + 24, height * scale + 24), (5, 8, 12))
    draw = ImageDraw.Draw(image)
    y = 12
    for line in lines:
        text_width = sum(glyph_widths[char] for char in line) + (len(line) - 1) * GAP
        x = 12 + ((width - text_width) * scale) // 2
        for char in line:
            rows = font_rows[char]
            glyph_width = glyph_widths[char]
            for row_index, bits in enumerate(rows):
                col = 0
                while col < glyph_width:
                    while col < glyph_width and (bits & (1 << (glyph_width - 1 - col))) == 0:
                        col += 1
                    start = col
                    while col < glyph_width and (bits & (1 << (glyph_width - 1 - col))) != 0:
                        col += 1
                    if col > start:
                        draw.rectangle(
                            [x + start * scale, y + row_index * scale, x + col * scale - 1, y + (row_index + 1) * scale - 1],
                            fill=(240, 244, 248),
                        )
            x += (glyph_width + GAP) * scale
        y += (TARGET_HEIGHT + line_gap) * scale
    image.save(PREVIEW_OUT)


def write_header():
    HEADER_OUT.write_text(
        """#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

bool numericClockCanDraw(const String& text);
int numericClockTextWidth(const String& text);
int numericClockTextWidth(const String& text, uint8_t scale);
int numericClockTextWidth(const String& text, uint8_t xNumerator, uint8_t xDenominator);
int numericClockTextHeight(uint8_t scale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t scale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xScale, uint8_t yScale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xNumerator, uint8_t xDenominator, uint8_t yScale);
""",
        encoding="utf-8",
    )


def write_cpp(font_rows, glyph_widths):
    arrays = []
    for char in CHARS:
        name = c_identifier(char)
        values = ",\n".join(f"  0b{row:0{glyph_widths[char]}b}UL" for row in font_rows[char])
        arrays.append(f"const uint32_t GLYPH_{name}[GLYPH_H] PROGMEM = {{\n{values}\n}};")

    arrays_text = "\n\n".join(arrays)
    glyph_entries = ",\n".join(
        f"  {{'{char}', {glyph_widths[char]}, GLYPH_{c_identifier(char)}}}" for char in CHARS
    )
    CPP_OUT.write_text(
        f"""// Generated by tools/generate_orbitron_clock_font.py from Orbitron.
#include "numeric_clock_font.h"

#include <pgmspace.h>

namespace
{{
const uint8_t GLYPH_H = {TARGET_HEIGHT};
const uint8_t GAP = {GAP};

struct Glyph
{{
  char value;
  uint8_t width;
  const uint32_t* rows;
}};

{arrays_text}

const Glyph GLYPHS[] PROGMEM = {{
{glyph_entries}
}};

bool glyphFor(char c, Glyph& glyph)
{{
  for (uint8_t i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++)
  {{
    memcpy_P(&glyph, GLYPHS + i, sizeof(Glyph));
    if (glyph.value == c) return true;
  }}
  return false;
}}

int scaleX(int value, uint8_t numerator, uint8_t denominator)
{{
  if (denominator == 0) denominator = 1;
  return (value * numerator) / denominator;
}}

int scaleXCeil(int value, uint8_t numerator, uint8_t denominator)
{{
  if (denominator == 0) denominator = 1;
  return (value * numerator + denominator - 1) / denominator;
}}

void drawRows(TFT_eSPI& tft, const uint32_t* rows, uint8_t width, int x, int y, uint16_t color, uint8_t xNumerator, uint8_t xDenominator, uint8_t yScale)
{{
  for (uint8_t row = 0; row < GLYPH_H; row++)
  {{
    uint32_t bits = pgm_read_dword(rows + row);
    uint8_t col = 0;
    while (col < width)
    {{
      while (col < width && (bits & (1UL << (width - 1 - col))) == 0) col++;
      uint8_t start = col;
      while (col < width && (bits & (1UL << (width - 1 - col))) != 0) col++;
      if (col > start)
      {{
        int x0 = x + scaleX(start, xNumerator, xDenominator);
        int x1 = x + scaleXCeil(col, xNumerator, xDenominator);
        tft.fillRect(x0, y + row * yScale, x1 - x0, yScale, color);
      }}
    }}
    if ((row & 0x07) == 0x07) yield();
  }}
}}
}}

bool numericClockCanDraw(const String& text)
{{
  Glyph glyph;
  for (unsigned int i = 0; i < text.length(); i++)
  {{
    if (!glyphFor(text[i], glyph)) return false;
  }}
  return text.length() > 0;
}}

int numericClockTextWidth(const String& text)
{{
  return numericClockTextWidth(text, 2);
}}

int numericClockTextWidth(const String& text, uint8_t scale)
{{
  return numericClockTextWidth(text, scale, 1);
}}

int numericClockTextWidth(const String& text, uint8_t xNumerator, uint8_t xDenominator)
{{
  if (text.length() == 0) return 0;
  int width = 0;
  Glyph glyph;
  for (unsigned int i = 0; i < text.length(); i++)
  {{
    if (i > 0) width += GAP;
    if (glyphFor(text[i], glyph)) width += glyph.width;
  }}
  return scaleXCeil(width, xNumerator, xDenominator);
}}

int numericClockTextHeight(uint8_t scale)
{{
  return GLYPH_H * scale;
}}

void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg)
{{
  numericClockDrawText(tft, text, x, y, color, bg, 2);
}}

void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t scale)
{{
  numericClockDrawText(tft, text, x, y, color, bg, scale, scale);
}}

void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xScale, uint8_t yScale)
{{
  numericClockDrawText(tft, text, x, y, color, bg, xScale, 1, yScale);
}}

void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xNumerator, uint8_t xDenominator, uint8_t yScale)
{{
  int cursor = 0;
  int height = numericClockTextHeight(yScale);
  Glyph glyph;
  for (unsigned int i = 0; i < text.length(); i++)
  {{
    if (!glyphFor(text[i], glyph)) continue;
    int cursorX = x + scaleX(cursor, xNumerator, xDenominator);
    int glyphW = scaleXCeil(glyph.width, xNumerator, xDenominator);
    tft.fillRect(cursorX, y, glyphW, height, bg);
    drawRows(tft, glyph.rows, glyph.width, cursorX, y, color, xNumerator, xDenominator, yScale);
    cursor += glyph.width + GAP;
    yield();
  }}
}}
""",
        encoding="utf-8",
    )


def main():
    font_path = find_font_path()
    font = load_font_for_height(font_path, TARGET_HEIGHT)
    glyphs = {char: render_char(font, char) for char in CHARS}
    digit_width = max(glyphs[char].width for char in "0123456789")
    colon_width = glyphs[":"].width
    if digit_width > MAX_GLYPH_WIDTH:
        raise ValueError(f"Orbitron digit width {digit_width}px exceeds {MAX_GLYPH_WIDTH}px")

    glyph_widths = {char: digit_width for char in "0123456789"}
    glyph_widths[":"] = colon_width
    font_rows = {char: rows_for(glyph, glyph_widths[char]) for char, glyph in glyphs.items()}
    write_header()
    write_cpp(font_rows, glyph_widths)
    render_preview(font_rows, glyph_widths)
    print(f"Generated {HEADER_OUT}")
    print(f"Generated {CPP_OUT}")
    print(f"Generated {PREVIEW_OUT}")
    print(f"Font: {font_path.name}, digit size: {digit_width}x{TARGET_HEIGHT}, colon width: {colon_width}")


if __name__ == "__main__":
    main()
