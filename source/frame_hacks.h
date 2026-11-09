#pragma once
unsigned penguin_secret_frame(const unsigned look[6],unsigned action);
unsigned penguin_outfit_frame(const unsigned look[6],unsigned action);
/* Asset ids for a secret action frame: avatars/<n>.gpa and outfits/<n>.gpa (0 for an empty slot). */
unsigned penguin_outfit_avatar(unsigned frame);
unsigned penguin_outfit_item(unsigned item,unsigned frame);
