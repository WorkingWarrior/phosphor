#pragma once

// U8g2 drawing onto an RGB565 canvas: pages are drawn in 1-bit with U8g2's fonts and primitives, and sendBuffer()
// expands them into the canvas with a foreground and background color.
// The "panel" is a plain memory buffer that the board then pushes to the screen.

#include <Arduino.h>
#include <U8g2lib.h>

class U8g2Canvas {
 public:
  U8g2Canvas();
  ~U8g2Canvas();

  // canvas: width x height RGB565 pixels (native endian) owned by the caller.
  bool begin(uint16_t *canvas, int width, int height);
  void setColors(uint16_t fg, uint16_t bg)
  {
    fg_ = fg;
    bg_ = bg;
  }
  U8G2 *u8g2() { return &u8g2_; }
  // Paints only the pixels U8g2 has set onto dst (width x height RGB565) in one color, leaving the rest of dst as it
  // is: text and shapes over a picture, one color per layer.
  void paint(uint16_t *dst, uint16_t color) const;

 private:
  static uint8_t displayCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
  static uint8_t byteCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

  U8G2 u8g2_;
  uint8_t *buf_ = nullptr;
  uint16_t *canvas_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  uint16_t fg_ = 0xFFFF;
  uint16_t bg_ = 0x0000;
};
