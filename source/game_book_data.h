#pragma once

#define BOOK_COUNT 3

typedef struct { const char *id,*title; unsigned room,pages; const char *const *texts; } BookData;

static const char *const BOOK1_TEXTS[] = {
  "My puffle is blue and likes to go for long walks.",
  "It likes to go to The Dock to look for penguins wearing blue just like me.",
  "One day when we were walking, my pet found two blue penguins with boots on their heads!",
  "My blue puffle and I started giggling because it was so silly.",
  "Then we both put boots on our heads and spent the rest of the day at The Dock giggling and having fun!",
  "It was the best day at The Dock ever! The End",
};

static const char *const BOOK2_TEXTS[] = {
  "It was just another night of fun for me and my buddies hanging out at the Ice Rink. Just as I was about to score a goal, all the lights burned out! The rink turned darker than a solar eclipse on Halloween!",
  "Luckily, I had my flashlight to find my buddy and ask where to find some lights. \"I'm glad I found you! Where could I find some new light bulbs?\" I asked when I found my buddy. She suggested that I look for them at the Lighthouse.",
  "I arrived at the Lighthouse to find a penguin pounding away at the drums very loudly. I stood on a chair and",
  "At the Ski Lodge there was a group of penguins playing Find Four upside-down. One replied to me, \"I saw this",
  "The diver penguin in the Attic was",
  "At the",
  "I opened the",
  "When I opened the",
  "When I got to the rink, the",
  "Then I remembered what day it was... It was the silliest April Fool's Day I ever had! The End",
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

static const BookData BOOK_DATA[] = {
  {"book1","My Puffle",917,6,BOOK1_TEXTS},
  {"book2","Burnt Out Bulbs",918,10,BOOK2_TEXTS},
  {"book3","Lime Green Dojo Clean",919,10,BOOK3_TEXTS},
};
