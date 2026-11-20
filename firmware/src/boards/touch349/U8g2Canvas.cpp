#ifdef BOARD_TOUCH349

#include "U8g2Canvas.h"

static U8g2Canvas *g_canvas = nullptr;

// Tile geometry is filled in begin(); u8g2 copies the pointer, so the struct has to outlive the object.
static u8x8_display_info_t g_info = {};

U8g2Canvas::U8g2Canvas()
{
  g_canvas = this;
}

U8g2Canvas::~U8g2Canvas()
{
  free(buf_);
  if (g_canvas == this) {
    g_canvas = nullptr;
  }
}

bool U8g2Canvas::begin(uint16_t *canvas, int width, int height)
{
  canvas_ = canvas;
  width_ = width;
  height_ = height;

  const int tile_w = (width + 7) / 8;
  const int tile_h = (height + 7) / 8;
  g_info = {};
  g_info.tile_width = tile_w;
  g_info.tile_height = tile_h;
  g_info.pixel_width = width;
  g_info.pixel_height = height;

  buf_ = (uint8_t *)ps_calloc(tile_w * 8, tile_h);
  if (!buf_) {
    return false;
  }

  u8g2_t *u = u8g2_.getU8g2();
  u8x8_Setup(u8g2_GetU8x8(u), displayCb, u8x8_dummy_cb, byteCb, u8x8_dummy_cb);
  u8g2_SetupBuffer(u, buf_, tile_h, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
  u8g2_InitDisplay(u);
  u8g2_SetPowerSave(u, 0);
  return true;
}

// The buffer is U8g2's vertical-top-lsb layout: one byte per column in each 8-row band, bit 0 = top row.
void U8g2Canvas::paint(uint16_t *dst, uint16_t color) const
{
  const int band_bytes = ((width_ + 7) / 8) * 8;
  for (int y = 0; y < height_; y++) {
    const uint8_t *band = buf_ + (y / 8) * band_bytes;
    const uint8_t bit = 1 << (y & 7);
    uint16_t *row = dst + y * width_;
    for (int x = 0; x < width_; x++) {
      if (band[x] & bit) {
        row[x] = color;
      }
    }
  }
}

uint8_t U8g2Canvas::byteCb(u8x8_t *, uint8_t, uint8_t, void *)
{
  return 1;
}

uint8_t U8g2Canvas::displayCb(u8x8_t *u8x8, uint8_t msg, uint8_t, void *arg_ptr)
{
  switch (msg) {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &g_info);
      return 1;

    case U8X8_MSG_DISPLAY_INIT:
      return 1;

    case U8X8_MSG_DISPLAY_DRAW_TILE: {
      U8g2Canvas *self = g_canvas;
      if (!self) {
        return 0;
      }
      const u8x8_tile_t *tile = (const u8x8_tile_t *)arg_ptr;
      // Each tile is 8 bytes: one byte per pixel column, bit 0 = top row.
      for (int t = 0; t < tile->cnt; t++) {
        for (int c = 0; c < 8; c++) {
          const int x = (tile->x_pos + t) * 8 + c;
          if (x >= self->width_) {
            break;
          }
          const uint8_t bits = tile->tile_ptr[t * 8 + c];
          for (int r = 0; r < 8; r++) {
            const int y = tile->y_pos * 8 + r;
            if (y >= self->height_) {
              break;
            }
            self->canvas_[y * self->width_ + x] = (bits >> r) & 1 ? self->fg_ : self->bg_;
          }
        }
      }
      return 1;
    }

    default:
      return 0;
  }
}

#endif  // BOARD_TOUCH349
