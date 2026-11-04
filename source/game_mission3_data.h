#pragma once
#include <stdint.h>

#define MISSION3_STEPS 11
#define MISSION3_SCENES 7

typedef struct {
 uint8_t scene;
 const char *title,*text,*objective,*action;
 uint8_t fuse,target;
} Mission3Step;

static const char *const MISSION3_SCENE_KEYS[]={"vault","office","hq","roof","boiler","fusebox","nightclub"};
static const char *const MISSION3_SCENE_PACKS[]={
 "missions/3/vault.gpk","missions/3/office.gpk","missions/3/hq.gpk","missions/3/roof.gpk",
 "missions/3/boiler.gpk","missions/3/fusebox.gpk","missions/3/nightclub.gpk",
};

static const Mission3Step MISSION3_STEP_DATA[]={
 {0,"Vault","Thank goodness you're here! The coins have been stolen from the vault and we have to get them back. I'm new to this whole Secret Agent thing and was told to guard the vault until another agent arrived.","Ask the rookie what happened.","Ask about the vault",0,0},
 {1,"Gift Shop manager's office","The manager's desk is covered in receipts. One drawer is locked, but a note on the desk mentions a vault combination kept for the guards.","Find the vault combination.","Search the desk",0,0},
 {0,"Vault","You found the combination. The rookie is still guarding the closed vault door.","Enter the combination into the vault.","Enter the combination",0,0},
 {0,"Vault","The vault door swings open. The coins are not on the floor; they are stuck to the ceiling!","Look at the ceiling.","Look at the ceiling",0,0},
 {2,"HQ","Thanks for coming, Agent. I've managed to unscramble some video surveillance footage from the Gift Shop. I think you'll find it helpful in your investigation. Would you like to watch it?","Watch the surveillance footage.","Watch the footage",0,0},
 {2,"HQ","Look up, and try the roof. The rooftop door might be locked. All the master keys are in this drawer; look for the one marked rooftop.","Take the rooftop key.","Open the drawer",0,0},
 {3,"Gift Shop roof","A giant electromagnet hums on the roof. It is pulling every metal object toward it, including the missing coins below.","Disable the magnet.","Disable the magnet",0,0},
 {2,"HQ","Good job in recovering the coins! The power in the Night Club has gone off. It probably happened when you deactivated the magnet. In the Boiler Room is a fuse box; reset the fuses to turn the power back on.","Go to the Boiler Room.","Accept the fuse box mission",0,0},
 {4,"Boiler Room","The fuse box is dark and the panel is loose. Four fuses sit behind the glass.","Open the fuse box.","Open the fuse box",0,0},
 {5,"Fuse box","Set the fuses to the pattern shown on the panel: ON, OFF, ON, OFF. Each switch also flips its neighbours.","Set the fuse pattern.","Power restored",1,5},
 {6,"Night Club","Wow! Thank you so much for turning the power back on. Now I can dance again.","Finish the mission.","Finish mission",0,0},
};
