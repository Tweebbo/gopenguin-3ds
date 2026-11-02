#pragma once
#include <stdbool.h>
typedef struct {
 float x,y,touchX,touchY,startX,startY;
 bool tracking,dragging;
} BookCamera;
void book_camera_reset(BookCamera *camera,float x,float y);
void book_camera_cancel(BookCamera *camera);
void book_camera_begin(BookCamera *camera,float x,float y);
void book_camera_drag(BookCamera *camera,float x,float y,float scaleX,float scaleY);
bool book_camera_end(BookCamera *camera);
void book_camera_pan(BookCamera *camera,float dx,float dy,float seconds);
void book_camera_clamp(BookCamera *camera,float left,float top,float right,float bottom,float halfWidth,float halfHeight);
