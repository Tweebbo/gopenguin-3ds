#include "game_beans_colour.h"
#include "game_beans_colours.h"
void game_beans_colour(SWLScene *scene,uint32_t rgb){
 SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
 for(unsigned i=0;i<BEANS_COLOUR_PATH_COUNT;i++)swl_color(scene,BEANS_COLOUR_PATHS[i],colour);
}
