/* Known-answer oracle for Cinder's bundled FreeType: fixed input -> asserted value. */
#include <stdio.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BITMAP_H

static int g_fail;

static void check_long(const char *name, long got, long want)
{
  if (got == want) printf("PASS %s\n", name);
  else { printf("FAIL %s: got %ld want %ld\n", name, got, want); g_fail++; }
}

static void check_str(const char *name, const char *got, const char *want)
{
  if (got && !strcmp(got, want)) printf("PASS %s\n", name);
  else { printf("FAIL %s: got '%s' want '%s'\n", name, got ? got : "(null)", want); g_fail++; }
}

static long bitmap_sum(const FT_Bitmap *b)
{
  long sum = 0;
  for (unsigned r = 0; r < b->rows; r++)
    for (unsigned c = 0; c < b->width; c++)
      sum += b->buffer[(long)r * b->pitch + c];
  return sum;
}

static const char bdf_font[] =
  "STARTFONT 2.1\n"
  "FONT -Oracle-Tiny-Medium-R-Normal--8-80-75-75-C-80-ISO8859-1\n"
  "SIZE 8 75 75\n"
  "FONTBOUNDINGBOX 4 4 0 0\n"
  "STARTPROPERTIES 3\n"
  "FAMILY_NAME \"Oracle Tiny\"\n"
  "FONT_ASCENT 4\n"
  "FONT_DESCENT 0\n"
  "ENDPROPERTIES\n"
  "CHARS 1\n"
  "STARTCHAR A\n"
  "ENCODING 65\n"
  "SWIDTH 500 0\n"
  "DWIDTH 5 0\n"
  "BBX 4 4 0 0\n"
  "BITMAP\n"
  "F0\n"
  "90\n"
  "90\n"
  "F0\n"
  "ENDCHAR\n"
  "ENDFONT\n";

int main(int argc, char **argv)
{
  FT_Library lib;
  FT_Face face;
  FT_Int maj, min, pat;
  const char *ttf = argc > 1 ? argv[1] : "samples/Kaleidoscope/resources/Asap-Regular.ttf";

  if (FT_Init_FreeType(&lib)) { printf("FAIL init\n"); return 1; }

  FT_Library_Version(lib, &maj, &min, &pat);
  check_long("freetype-version", maj * 10000 + min * 100 + pat, 20901);

  if (FT_New_Face(lib, ttf, 0, &face)) { printf("FAIL open-ttf\n"); return 1; }
  check_str("ttf-family", face->family_name, "Asap");
  check_str("ttf-style", face->style_name, "Regular");
  check_long("ttf-num-glyphs", face->num_glyphs, 252);
  check_long("ttf-units-per-em", face->units_per_EM, 1000);

  FT_UInt gi = FT_Get_Char_Index(face, 'A');
  check_long("ttf-cmap-A", gi, 36);
  FT_Set_Char_Size(face, 20 * 64, 20 * 64, 72, 72);
  if (FT_Load_Glyph(face, gi, FT_LOAD_NO_HINTING)) { printf("FAIL load-A\n"); g_fail++; }
  else {
    check_long("ttf-advance-A", face->glyph->metrics.horiAdvance, 785);
    check_long("ttf-outline-points-A", face->glyph->outline.n_points, 29);
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) { printf("FAIL render-A\n"); g_fail++; }
    else {
      check_long("ttf-bitmap-width-A", face->glyph->bitmap.width, 13);
      check_long("ttf-bitmap-rows-A", face->glyph->bitmap.rows, 16);
      check_long("ttf-bitmap-sum-A", bitmap_sum(&face->glyph->bitmap), 14369);
    }
  }
  FT_Done_Face(face);

  if (FT_New_Memory_Face(lib, (const FT_Byte *)bdf_font, sizeof bdf_font - 1, 0, &face)) {
    printf("FAIL open-bdf\n"); g_fail++;
  } else {
    check_str("bdf-family", face->family_name, "Oracle Tiny");
    check_long("bdf-bitmap-A", FT_Load_Char(face, 'A', FT_LOAD_RENDER), 0);
    check_long("bdf-advance-A", face->glyph->advance.x >> 6, 5);
    check_long("bdf-rows-A", face->glyph->bitmap.rows, 4);
    check_long("bdf-pixel-mode", face->glyph->bitmap.pixel_mode, FT_PIXEL_MODE_MONO);
    check_long("bdf-row0", face->glyph->bitmap.buffer[0], 0xF0);
    check_long("bdf-row1", face->glyph->bitmap.buffer[face->glyph->bitmap.pitch], 0x90);
    FT_Done_Face(face);
  }

  {
    /* 10x10 pixel square (26.6 coordinates) through the scan converter */
    FT_Vector pts[4] = { {0, 0}, {640, 0}, {640, 640}, {0, 640} };
    char tags[4] = { FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON };
    short contours[1] = { 3 };
    FT_Outline ol = { 1, 4, pts, tags, contours, FT_OUTLINE_NONE };
    unsigned char buf[16 * 16];
    FT_Bitmap bm = { 16, 16, 16, buf, 256, FT_PIXEL_MODE_GRAY, 0, NULL };
    memset(buf, 0, sizeof buf);
    check_long("raster-gray-status", FT_Outline_Get_Bitmap(lib, &ol, &bm), 0);
    check_long("raster-gray-sum", bitmap_sum(&bm), 10 * 10 * 255);

    unsigned char mbuf[16 * 2];
    FT_Bitmap mm = { 16, 16, 2, mbuf, 2, FT_PIXEL_MODE_MONO, 0, NULL };
    memset(mbuf, 0, sizeof mbuf);
    check_long("raster-mono-status", FT_Outline_Get_Bitmap(lib, &ol, &mm), 0);
    long bits = 0;
    for (int i = 0; i < 32; i++) for (int b = 0; b < 8; b++) bits += (mbuf[i] >> b) & 1;
    check_long("raster-mono-bits", bits, 100);
  }

  FT_Done_FreeType(lib);
  printf("DONE\n");
  return g_fail ? 1 : 0;
}
