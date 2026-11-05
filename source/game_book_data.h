#pragma once

#define BOOK_COUNT 3
#define BOOK_PAINT_MAX 21

/** Original paint progression: when the page text reaches `at` characters the page art goes to `frame` (0 = end). */
typedef struct { unsigned short at,frame; const char *path; } BookPaint;
typedef struct { const char *id,*title; unsigned room,pages; const char *const *texts; const BookPaint (*paint)[BOOK_PAINT_MAX]; } BookData;

static const char *const BOOK1_TEXTS[] = {
  "My puffle is blue and likes to go for long walks.",
  "It likes to go to The Dock to look for penguins wearing blue just like me.",
  "One day when we were walking, my pet found two blue penguins with boots on their heads!",
  "My blue puffle and I started giggling because it was so silly.",
  "Then we both put boots on our heads and spent the rest of the day at The Dock giggling and having fun!",
  "It was the best day at The Dock ever! The End",
};

static const BookPaint BOOK1_PAINT[][BOOK_PAINT_MAX] = {
  {{18,2,""},{18,2,"puff"},{49,2,"puff.puff"},{0,0,NULL}},
  {{27,2,""},{61,3,""},{74,4,""},{0,0,NULL}},
  {{61,2,""},{72,6,""},{87,7,""},{0,0,NULL}},
  {{15,2,""},{21,3,""},{38,4,""},{0,0,NULL}},
  {{23,2,""},{36,3,""},{87,4,""},{0,0,NULL}},
  {{45,2,"shadows"},{0,0,NULL}},
};

static const char *const BOOK2_TEXTS[] = {
  "It was just another night of fun for me and my buddies hanging out at the Ice Rink. Just as I was about to score a goal, all the lights burned out! The rink turned darker than a solar eclipse on Halloween!",
  "Luckily, I had my flashlight to find my buddy and ask where to find some lights. \"I'm glad I found you! Where could I find some new light bulbs?\" I asked when I found my buddy. She suggested that I look for them at the Lighthouse.",
  "I arrived at the Lighthouse to find a penguin pounding away at the drums very loudly. I stood on a chair and jumped until he stopped playing. He said that the Ski Lodge would have some bulbs.",
  "At the Ski Lodge there was a group of penguins playing Find Four upside-down. One replied to me, \"I saw this DJ penguin put some in their backpack when I was up in the Attic.\"",
  "The diver penguin in the Attic was nervous to see me looking around. \"All the light bulbs are gone after that DJ took them! He may have some left in their igloo!\"",
  "At the DJ penguin's igloo, I asked if he had any spare bulbs. To my surprise, he pulled out a barrel and handed it to me! I was so excited, I quickly ran back to the Ice Rink!",
  "I opened the barrel and took out the lights. But when I put them in the light posts, they started flashing colors everywhere! Red! Blue! Yellow! Now three colors instead of one lit up the Ice Rink!",
  "When I opened the barrel the viking gave me, out popped balls of fire that lit up the Ice Rink in a red glow! All my buddies put on viking hats and danced around the rink singing sea shanties!",
  "When I got to the rink, the barrel the alien gave me started glowing lime green. The lights floated out by themselves! My buddies celebrated by dressing up like robots and dancing like chickens.",
  "Then I remembered what day it was... It was the silliest April Fool's Day I ever had! The End",
};

static const BookPaint BOOK2_PAINT[][BOOK_PAINT_MAX] = {
  {{40,2,""},{55,3,""},{84,4,""},{121,5,""},{148,6,""},{205,7,""},{0,0,NULL}},
  {{19,2,""},{46,3,""},{177,4,""},{230,5,""},{0,0,NULL}},
  {{28,2,""},{73,2,"otherpenguin"},{86,3,"otherpenguin"},{116,2,"penguin"},{142,4,"otherpenguin"},{142,4,"penguin"},{191,5,"otherpenguin"},{0,0,NULL}},
  {{17,2,""},{47,2,"penguins"},{64,3,"penguins"},{78,4,"penguins"},{175,5,"penguins"},{0,0,NULL}},
  {{18,8,"otherpenguin"},{31,2,""},{43,9,"otherpenguin"},{113,11,"otherpenguin"},{0,0,NULL}},
  {{27,2,""},{27,2,"otherpenguin"},{62,2,"penguin"},{101,3,"penguin"},{175,4,"penguin"},{0,0,NULL}},
  {{20,2,"penguin"},{20,3,"container"},{45,3,"penguin"},{45,4,"container"},{85,4,"penguin"},{130,5,"penguin"},{130,2,"rink"},{130,2,"rinkbg"},{136,7,"penguin"},{136,4,"rink"},{136,4,"rinkbg"},{145,9,"penguin"},{145,6,"rink"},{145,6,"rinkbg"},{197,11,"penguin"},{197,8,"rink"},{197,8,"rinkbg"},{197,2,"penguin.lights.lights.light1"},{197,2,"penguin.lights.lights.light2"},{197,2,"penguin.lights.lights.light3"},{0,0,NULL}},
  {{25,2,"penguin"},{25,2,"otherpenguins"},{25,3,"container"},{70,3,"penguin"},{70,3,"otherpenguins"},{70,2,"rink"},{110,4,"penguin"},{110,4,"otherpenguins"},{110,3,"rink"},{144,7,"otherpenguins"},{192,8,"otherpenguins"},{0,0,NULL}},
  {{81,2,"penguin"},{81,2,"otherpenguins"},{81,3,"container"},{119,3,"penguin"},{119,3,"otherpenguins"},{119,2,"rink"},{119,4,"container"},{168,4,"penguin"},{168,4,"otherpenguins"},{194,5,"penguin"},{194,5,"otherpenguins"},{0,0,NULL}},
  {{93,3,""},{0,0,NULL}},
};

static const char *const BOOK3_TEXTS[] = {
  "I am the busiest penguin I know Cleaning the windows at the Dojo Wiping them down and getting them shined Making sure no one has left streaks behind",
  "Then one day while I was working away Wouldn't you know it, some penguins they came And brought this container as big as a tree And filled to the top with glowing lime green!",
  "I gazed in amazement as they jumped in Then one of them spoke to me with a grin, \"It's a colorful party for all to express!\" But while he was so happy, the place was a mess!",
  "There was paint on the walls and paint on the ceiling This paint-splattered place was so unappealing! And just when I thought I could take it no more A penguin began to make tracks on the floor!",
  "I just cleaned the floor!\" I loudly proclaimed, \"And now I will have to go mop it again!\" But wouldn't you know it, this mop that I used Was dropped in the bucket of lime-colored ooze!",
  "Now I faced a decision of just what to do My mop was unusable thanks to this crew The rags that I used were just far too worn out So I stood on that paint can and started to shout.",
  "Then a penguin responded, \"You seem so upset! It's time for a party, but did you forget? Perhaps there's a way for me to help you. And I think that I know just what we should do!\"",
  "The penguin she pointed towards a large box Containing some capes and a couple old socks. \"Let's use up these capes to clean up the mess!\" Now that's an idea I wouldn't have guessed!",
  "Then that penguin came up with an idea so great, \"Why use just two flippers when you can use eight?\" Then an octopus came to lend us a hand And at least seven more! Now, isn't that grand?",
  "When the party was over, then everyone joined! They helped clean the Dojo without needing a coin. And when we were done, the place was so perfect You'd never have guessed it was once a green wreck! The End",
};

static const BookPaint BOOK3_PAINT[][BOOK_PAINT_MAX] = {
  {{17,2,"penguin"},{65,4,"penguin"},{65,2,"dojo"},{106,5,"penguin"},{148,6,"penguin"},{148,3,"dojo"},{0,0,NULL}},
  {{38,2,"penguin"},{84,3,"penguin"},{84,2,"otherpenguins"},{111,3,"otherpenguins"},{128,6,"otherpenguins"},{174,7,"otherpenguins"},{0,0,NULL}},
  {{39,2,"penguin"},{39,3,"dojo"},{82,3,"penguin"},{173,4,"dojo"},{173,4,"penguin"},{0,0,NULL}},
  {{29,2,"penguin"},{54,3,"penguin"},{102,4,"penguin"},{194,6,"penguin"},{0,0,NULL}},
  {{49,2,""},{90,3,""},{149,4,""},{184,5,""},{0,0,NULL}},
  {{180,2,""},{0,0,NULL}},
  {{27,6,""},{179,7,""},{0,0,NULL}},
  {{91,2,""},{139,3,""},{182,4,""},{0,0,NULL}},
  {{101,2,""},{122,3,""},{140,4,""},{165,5,""},{187,6,""},{0,0,NULL}},
  {{47,8,""},{98,9,""},{198,10,""},{205,11,""},{0,0,NULL}},
};

static const BookData BOOK_DATA[] = {
  {"book1","My Puffle",917,6,BOOK1_TEXTS,BOOK1_PAINT},
  {"book2","Burnt Out Bulbs",918,10,BOOK2_TEXTS,BOOK2_PAINT},
  {"book3","Lime Green Dojo Clean",919,10,BOOK3_TEXTS,BOOK3_PAINT},
};
