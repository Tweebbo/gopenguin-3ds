#include "book_camera.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void near(float a,float b){assert(fabsf(a-b)<.002f);}
int main(void){
 BookCamera camera;book_camera_reset(&camera,450,280);
 for(unsigned i=0;i<3600;i++)book_camera_pan(&camera,.08f,-.1f,1.f/60);
 near(camera.x,450);near(camera.y,280);
 book_camera_begin(&camera,40,90);book_camera_drag(&camera,40,90,.4f,.4f);book_camera_drag(&camera,42,91,.4f,.4f);
 near(camera.x,450);near(camera.y,280);assert(book_camera_end(&camera));
 book_camera_begin(&camera,40,90);book_camera_drag(&camera,60,100,.4f,.4f);
 near(camera.x,500);near(camera.y,305);assert(camera.dragging);
 book_camera_pan(&camera,-1,0,.1f);near(camera.x,500);
 for(unsigned i=0;i<100;i++)book_camera_drag(&camera,60,100,.4f,.4f);
 near(camera.x,500);near(camera.y,305);assert(!book_camera_end(&camera));
 for(unsigned i=0;i<600;i++)book_camera_pan(&camera,0,0,1.f/60);
 near(camera.x,500);near(camera.y,305);
 book_camera_pan(&camera,1,0,.1f);near(camera.x,524);near(camera.y,305);
 book_camera_begin(&camera,20,90);book_camera_pan(&camera,-1,0,.1f);near(camera.x,500);
 book_camera_drag(&camera,40,90,.4f,.4f);near(camera.x,550);assert(!book_camera_end(&camera));
 book_camera_cancel(&camera);near(camera.x,550);near(camera.y,305);
 book_camera_clamp(&camera,0,0,760,480,250,150);near(camera.x,510);near(camera.y,305);
 book_camera_pan(&camera,1,1,10);book_camera_clamp(&camera,0,0,760,480,250,150);near(camera.x,510);assert(camera.y<=330);
 book_camera_clamp(&camera,100,100,200,200,250,150);near(camera.x,150);near(camera.y,150);
 book_camera_begin(&camera,0,0);book_camera_drag(&camera,NAN,10,.4f,.4f);book_camera_pan(&camera,NAN,0,.1f);near(camera.x,150);
 book_camera_reset(&camera,380,240);assert(!camera.tracking&&!camera.dragging);near(camera.x,380);
 printf("PASS catalogue camera: 60 seconds of neutral drift filtered, tap/jitter stability, relative drag, stationary hold, stick handoff, retained centre, bounds and input cancellation\n");return 0;
}
