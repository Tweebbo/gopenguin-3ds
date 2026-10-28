#pragma once
// Lower/upper-screen chrome in the style of the Club Penguin mobile app (round blue
// gradient buttons with a white stroke and a soft drop shadow, white cards, a glass tab
// bar) with the pixel penguin micro-logo. Shapes only: main.c keeps its text and icons.
// Every element keeps the rectangle it is given, so touch areas do not move.
#include <stdbool.h>
#include <stdint.h>

typedef enum {UI_LOGO_SMALL=16,UI_LOGO_MEDIUM=24,UI_LOGO_LARGE=32} UILogoSize;

/** Once per frame, before drawing: reads the stylus for pressed states. */
void ui_frame(void);
/** True while the stylus is held inside the rectangle (bottom screen only). */
bool ui_touching(float x,float y,float w,float h);
void ui_round(float x,float y,float w,float h,float radius,uint32_t rgb,uint8_t alpha);
/** Rounded rectangle with a vertical gradient from top to bottom. */
void ui_round_gradient(float x,float y,float w,float h,float radius,uint32_t top,uint32_t bottom);
/** Soft sky gradient with faint snow for the lower screen. */
void ui_background(void);
/** Top bar of the lower screen, 27 px tall, with the micro-logo at the left; text starts at x 30. */
void ui_header(void);
/**
 * A panel in place of the old square box: 0x0085c7 is a raised blue button, 0xffffff a
 * white card, any other fill a rounded swatch of that colour. Pressed under the stylus.
 */
void ui_box(float x,float y,float w,float h,uint32_t fill);
/** Raised round button; returns whether it is drawn pressed. */
bool ui_button(float x,float y,float w,float h,bool pressed,bool enabled);
/** A minigame's lower-screen fill: the whole screen becomes a gradient of its colour, smaller fills round. */
void ui_fill(float x,float y,float w,float h,uint32_t rgb);
/** Bottom tab bar background (y 207..240) and the raised pill of the active tab. */
void ui_tab_bar(void);
void ui_tab(float x,float y,float w,float h,bool active);
/** Upper-screen label pill (room name) with the micro-logo; text starts at x + 24. */
void ui_label(float x,float y,float w,float h);
/** Pixel penguin; (x, y) is the top-left corner, scale 1 = one screen pixel per logo pixel. */
void ui_logo(float x,float y,UILogoSize size,float scale);
float ui_logo_width(UILogoSize size,float scale);
