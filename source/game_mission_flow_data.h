#pragma once
#include <stdint.h>

#define MISSION_FLOW_COUNT 8
#define MISSION_FLOW_MAX_SCENES 9
#define MISSION_FLOW_MAX_STEPS 10

typedef struct { uint8_t type,neighbours,count,target,order[8]; const char *code,*labels[8]; } MissionPuzzle;
typedef struct { uint8_t scene; const char *title,*text,*objective,*action; MissionPuzzle puzzle; } MissionStep;
typedef struct { uint8_t mission,sceneCount,stepCount; uint16_t room,reward; const char *title; const char *sceneKeys[MISSION_FLOW_MAX_SCENES]; const char *scenePacks[MISSION_FLOW_MAX_SCENES]; MissionStep steps[MISSION_FLOW_MAX_STEPS]; } MissionData;

static const MissionData MISSION4 = {
 4,9,10,913,300,"Avalanche Rescue",
 {"hq","gadget","lodge","lighthouse","beacon","groom","ski-village","mountain","cliff"},
 {"missions/4/hq.gpk","missions/4/gadget.gpk","missions/4/lodge.gpk","missions/4/lighthouse.gpk","missions/4/beacon.gpk","missions/4/groom.gpk","missions/4/ski-village.gpk","missions/4/mountain.gpk","missions/4/cliff.gpk"},
 {
  {0,"HQ","Good, you're here. I've been waiting for you to arrive. There has been an avalanche on the Ski Hill. Four penguins trapped on a cliff are in need of rescue. You'll need a retrieval device to reach them. Try adapting the life preserver shooter with another item. I would start by looking for something that will help to reel the preserver back in.","Ask G about the avalanche and the rescue plan.","Ask about the avalanche",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Gadget Room","The life preserver shooter sits on the top shelf of the case in the Gadget Room. G has unlocked the door so you can come and go as you need.","Take the life preserver shooter.","Take the shooter",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Ski Lodge","A fishing pole leans beside the back door that leads to the ice-fishing hole.","Take the fishing pole.","Take the fishing pole",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Lighthouse","A coil of rope hangs out of the rowboat in the Lighthouse.","Take the rope.","Take the rope",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {4,"Beacon","Wow! A portable telescope! Now you have to find a place to use it.","Take the telescope down from the railing.","Take the telescope",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"G's Room","Wow, that fits perfectly. Through the telescope you can see the paths from the mountain top down to four stranded penguins.","Look through the telescope and note the safe route down.","Look through the telescope",{3,0,5,0,{0,1,2,3,4,0,0,0},0,{"Left path","Center path","Right path","Left path","Center path"}}},
  {6,"Ski Village","That's strange. Looks like fur of some kind. G will be interested in this find. A crying penguin asks: I'm so bummed out! The ski lift is broken and I want to go for a ride. Do you know how to fix it?","Pick up the white fur.","Pick up the fur",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {7,"Mountain","Those are strange looking footprints. They don't look like they're from a penguin. The run splits left, center and right; a wrong turn sends you sliding back to the top.","Follow the telescope route down the mountain.","Take the route",{3,0,5,0,{0,1,2,3,4,0,0,0},0,{"Left","Center","Right","Left","Center"}}},
  {8,"Cliff","Quick, there are four penguins stranded on this cliff and you must find a way to get them to safety. Put the fishing pole on the life preserver shooter, then add the rope.","Build the retrieval contraption and rescue all four penguins.","Fire the preserver",{3,0,6,0,{0,1,2,3,4,5,0,0},0,{"Clear branches","Hook middle penguin","Hook right penguin","Reach the third","Lower to ledge","Swing for the fourth"}}},
  {7,"Mountain","This appears to be the same texture, color and weight as the other fur collected so far. More analysis will need to be conducted. I will submit this evidence to our Director, but I can tell you right now this fur is neither penguin nor puffle. Thank you for your excellent work Agent.","Give G the white fur.","Give the fur",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION5 = {
 5,7,10,914,300,"Secret of the Fur",
 {"gadget","analyzer","coffee","pizza","machine","beacon","behind-lodge"},
 {"missions/5/gadget.gpk","missions/5/analyzer.gpk","missions/5/coffee.gpk","missions/5/pizza.gpk","missions/5/machine.gpk","missions/5/beacon.gpk","missions/5/behind-lodge.gpk"},
 {
  {0,"Gadget Room","Good morning agent, good to see you again. Are you ready to get to the bottom of this strange fur mystery? I dug out my old Furensic Analyzer to check the fur, but I am unable to get it to work. Maybe you could take a look at it and see what's wrong.","Ask G about the fur and inspect the Furensic Analyzer.","Open the analyzer",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Furensic Analyzer","A tangled sample of pink fur is jammed in the machine. The spy comb untangles it; the analyzer matches it to a pink puffle. What a clever use of your spy comb! Now let us analyze the white fur. It seems this fur is from an unknown source and contains some strange trace elements. Bring larger samples of these elements back to me.","Untangle the pink puffle fur and scan the white fur.","Use the spy comb",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"Gadget Room","Before you leave, grab the AC 1000 from the Prototypes shelf. The fan will be useful for blowing something closer.","Take the AC 1000 fan.","Take the AC 1000",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Coffee Shop","Oh, hi there! You scared me! Sorry for the mess. I saw it this morning on my way into the shop. It was a big, strange, angry thing. It made lots of clicking and scratching sounds while it scrambled around. I would love to make some hot chocolate for you, but the machine is broken.","Ask the barista what happened and inspect the machine.","Inspect the machine",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Pizza Parlor","I walked through the kitchen door this morning and heard a loud CRASH! I noticed an odd shadow. This sounds strange, but it looked like a really hairy, mutant crab! Sure thing. Anything that might be of some help. Take what you need.","Take hot sauce and chocolate sauce.","Take the sauces",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {4,"Hot chocolate machine","The milk hose is disconnected, the dial is set to Cold and the mug is on top. Pour the chocolate sauce into the funnel, reconnect the milk, set the dial to Hot, place the mug under the spout and press pour.","Repair the machine and make the hot chocolate.","Fix the machine",{3,0,5,0,{0,1,2,3,4,0,0,0},0,{"Pour chocolate sauce","Reconnect milk","Set dial to Hot","Place the mug","Press pour"}}},
  {5,"Beacon","In the Lighthouse, take the rope from the rowboat and the fishing net by the wheel. At the foot of the stairs the penguins are scared. Up at the Beacon, the fuel can floats out of reach.","Take the rope and net, use the AC 1000 to blow the fuel can closer, then cut its string with the spy phone scissors.","Cut down the fuel",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"Gadget Room","Great job! Let us get started. Put each element into the calibration machine and then pick up the goggles. They should help you track down whatever it is that is causing all this trouble.","Pour in hot chocolate, hot sauce and jet pack fuel, then take the tracking goggles.","Calibrate the goggles",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Hot chocolate","Hot sauce","Jet pack fuel"}}},
  {6,"Behind the Ski Lodge","With the goggles on, paw prints lead from the Pizza Parlor through the Snow Forts, Coffee Shop, Dock and Lighthouse to the Ski Lodge. Behind the lodge: ACHOO! Brrrrr! Everything here is cold. GRRRR. A candle is what I need.","Take the candle, combine the rope and net into a trap, hang it on the branch and bait it.","Set the trap",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Rope + net","Hang the trap","Candle bait"}}},
  {0,"Gadget Room","A crab and a bear? That is strange. But that would explain the strange sightings and prints. Give me the crab; I have a few questions for him. Put it in the Furensic Analyzer. Oh my! That is not just any bear. That is a polar bear!","Hand over the crab and analyze the final fur sample.","Analyze the fur",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION6 = {
 6,6,10,915,300,"Questions for a Crab",
 {"gadget","ski-village","wilderness","cave","pizza","behind-lodge"},
 {"missions/6/gadget.gpk","missions/6/ski-village.gpk","missions/6/wilderness.gpk","missions/6/cave.gpk","missions/6/pizza.gpk","missions/6/behind-lodge.gpk"},
 {
  {0,"Gadget Room","Hello Agent, I'm glad you could come. I'm all set to ask the crab you captured a few questions. This machine should be able to translate the crab's clicking. Oh bother, the crab is getting away! Quick, Agent! Follow and catch him while I fix up this mess.","Start the Crab Translator 3000.","Start the translator",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Ski Village","Did you see that? Yes we did! We saw a crab jump on the ski lift and go up the hill. What a strange sight, a crab using a ski lift!","Ask the penguins where the crab went.","Ask the witnesses",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Wilderness","The crab scuttles off the back of the Mountain and into the wilderness. A sack of O berries hangs high in a tree. Your spy phone scissors can cut the string.","Follow the crab and cut down the O berry sack.","Cut the sack down",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Wilderness","A black puffle watches you hungrily. Give it an O berry and it will follow you. Put another berry on the small door and the puffle darts through and lets you in.","Befriend the black puffle and open the cave door.","Offer an O berry",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Herbert's cave","Well, well, well. What do we have here, Klutzy? It appears to have brought us a gift. How kind! I am Herbert P. Bear, Esquire, formerly of the Arctic Circle. Using your own technology, I have built a mechanical woodchopper. With it I will chop your cozy little Ski Lodge into a hundred pieces.","Listen to Herbert and learn his plan.","Listen to Herbert",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Herbert's cave","The puffle appears and looks at the cage lift. Three O berries on the mechanism will make it jump: the right handle starts the water, the left drain reverses it, and the right tray sends it to the waterwheel.","Place three O berries in order to lift the cage.","Place the berries",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Right handle","Left drain","Right tray"}}},
  {3,"Herbert's cave","G's Electro Magnet 3000 plans, a bottle of hot sauce, a rope and an anchor are among the junk. Hot sauce on an O berry makes a flaming berry; the puffle burns through the snow door. Rope plus anchor makes a grappling hook to climb the cliff.","Take the plans and supplies, melt the snow and climb out.","Escape the cave",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Take the plans","Flaming O berry","Rope and anchor"}}},
  {4,"Pizza Parlor","The pizza maker asks what you want. Order the Seaweed pizza for the hungry bear. One vegetarian seaweed pizza, coming up.","Order a Seaweed pizza.","Order the pizza",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"Behind the Ski Lodge","Herbert grabs the pizza: Alright! A seaweed pizza! Perfect for a vegetarian like me. While he eats, pull the lever from the green arrow to the red arrow. The woodchopper shoots backward into the frozen pond.","Distract Herbert and reverse the woodchopper.","Pull the lever",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"Behind the Ski Lodge","Well that was interesting. I look forward to hearing about all of this in your report. Did you find anything interesting on your journey? Thank you Agent. Superb job! You managed to locate the bear den and stop him before any real damage was done.","Return G's Electro Magnet 3000 plans.","Give G the plans",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION7 = {
 7,7,10,920,300,"Clockwork Repairs",
 {"hq","clock","gadget","berg","town","forts","theatre"},
 {"missions/7/hq.gpk","missions/7/clock.gpk","missions/7/gadget.gpk","missions/7/berg.gpk","missions/7/town.gpk","missions/7/forts.gpk","missions/7/theatre.gpk"},
 {
  {0,"HQ","Thank you for coming Agent. I am afraid we have a situation. I assume you are aware that the Clock Tower is broken?","Report to G in the HQ.","Ask G what happened",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","At 10:00, Penguin Standard Time, the Clock Tower experienced a fatal error, causing it to malfunction. As all other clocks on the island are directly linked to the Tower, many have gone haywire as a result. The polar bear! I should have known he was behind this!","Watch the surveillance footage on G's monitor.","Watch the footage",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Clock Tower","As you will see, we are missing some pretty important parts. Obviously the Prime Gear has been taken, but one of the main Anti-discombobulating Springs has been sprung too. And if that were not enough, the Snowball Powered Target-Winder has been pinched as well!","Ask Rory which parts the clock needs.","Ask about the clock",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Gadget Room","To open the lock on the gadget case, you need the right \"key\". Any agent should know what the \"key\" is.","Turn the three dials and open the gadget case.","Unlock the case",{2,0,3,0,{0,0,0,0,0,0,0,0},"KEY",{0}}},
  {3,"Iceberg","Looks like the spring is frozen in some ice, and just out of reach.","Pull the spring free with the Electromagnet 2000.","Use the electromagnet",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {4,"Town","I was trying to find some way to help out, so I am putting up missing posters of the gear around Town. Maybe someone has seen it. Wait, I have been looking for a picture of the gear! That poster is perfect. Oh really? That is wonderful! Here, take one!","Take a missing-gear poster from Rookie.","Take the poster",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"Snow Forts","You will have to prove yourself worthy. Hit the target three times, and we will make you an honorary member of Blue Team.","Fill a bucket with Snow Forts snow, then hit the target three times.","Throw snowballs",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {6,"The Stage","We have actually just run out of yellow puffles. I did hear that there is a wild one living at the Stage, but it is pretty shy. Yellow puffles are amazing! Get them a picture of something, and some stuff to make it with, and they will whip up just about anything.","Play the piano to coax out the yellow puffle, then give it the poster and the bucket of snow.","Show the picture and the snow",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Clock Tower","Alright, you did it! The Clock Tower is running again!","Fit the spring, the gear and the target into the clock.","Start the clock",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","Well well well, it looks like I misjudged you my fowl friend. Fixing the clock was quite the feat. But I am afraid that you are still too late. Stealing the gear from your silly clock was only the first stage of my plan. Excellent work Agent. Once again you have gotten Club Penguin out of a serious jam.","Report back to G.","Report to G",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION8 = {
 8,8,10,921,300,"Mysterious Tremors",
 {"gadget","dock","hq","town","plaza","coffee","tunnels","boiler"},
 {"missions/8/gadget.gpk","missions/8/dock.gpk","missions/8/hq.gpk","missions/8/town.gpk","missions/8/plaza.gpk","missions/8/coffee.gpk","missions/8/tunnels.gpk","missions/8/boiler.gpk"},
 {
  {0,"Gadget Room","Agent - thanks for coming so quickly. We have an unusual case for you today. Let us start at the beginning. As I am sure you know, tremors began about a week ago. They were thought to be earthquakes, but were so strange that the PSA began investigating.","Report to G in the Gadget Room.","Start the briefing",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"Gadget Room","This seismograph is keeping track of the earthquakes. When the next one hits, it will tell us where it is coming from, and we want you to investigate. Here we go! Good timing. According to these readings, that tremor started at the Docks.","Read the seismograph and head to the Docks.","Head to the Docks",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Docks","Do you like my drill? I have put that clock gear to good use, would you not say? AHH! The map! Klutzy, grab it! HAHAHA! No reverse this time! So long Agent!","Confront Herbert at the Docks.","Confront Herbert",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"HQ","Your mission now: discover Herbert's plan and stop him at any cost. Investigate the tunnels and track him down. Also, search for this missing map, it may be crucial.","Report to G and accept the new orders.","Accept the mission",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Town","I was just standing here when the ground started shaking! The next thing I know, the Gift Shop started sinking!","Float the Gift Shop back into place: balloons, net, pegs, cream soda.","Rig the rescue",{3,0,4,0,{0,1,2,3,0,0,0,0},0,{"Balloons","Net","Pegs","Cream soda"}}},
  {4,"Plaza","Though I wish I could have a delicious double meat pizza, extra grey fish right now. But I am not going to risk losing my place in line to get one. Gadzooks! Now that is the stuff! I was starving. Is there anything I can do for you in return?","Bring the newspaper penguin a pizza and take his paper.","Trade pizza for the newspaper",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"Coffee Shop","Would you look at this mess? Although it is not like this is the first time this place has been turned upside down. Hey, thanks again. Help yourself to some cookies. They are on the house!","Help clean up the Coffee Shop.","Clean up the cookies",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {6,"Tunnels","It is dark in here. You might need something to help you see.","Light the lantern and follow the tunnels to Herbert's drill.","Enter the tunnels",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {7,"Boiler Room","Hello Agent. I am getting some strange readings from the Boiler Room. Is everything all right? Do not worry about Herbert. Right now your priority is to fix the boiler. Klutzy, hand me that 7/16 wrench. Haha! Just a few more cranks and this beautiful boiler is ours Klutzy! Uh oh. Umm. I think it is time to be going, Klutzy. Come along.","Fix the boiler, then stop Herbert red-handed.","Fix the boiler",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"HQ","Excellent work Agent! Other field agents have been reporting from all over the island. It looks like you have prevented certain disaster. Thank you, Agent. This will not only make a good back up gear, but Herbert will not be able to use it for his plans any more.","Report to G and hand over the Prime Gear.","Report to G",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION9 = {
 9,9,10,922,300,"Operation: Spy and Seek",
 {"gadget","forest","beach","plaza","mountain","mineshack","berg","lodge","hq"},
 {"missions/9/gadget.gpk","missions/9/forest.gpk","missions/9/beach.gpk","missions/9/plaza.gpk","missions/9/mountain.gpk","missions/9/mineshack.gpk","missions/9/berg.gpk","missions/9/lodge.gpk","missions/9/hq.gpk"},
 {
  {0,"Gadget Room","The objective of your next mission is to gather intelligence on Herbert's whereabouts. We need you to find out as much as possible about his plans. In order to calculate the location of the spy phone, we will need to spread them out across the island: one as high as possible, one as low as possible, and one out at sea.","Report to G in the Gadget Room and take the three trackers.","Start the briefing",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Forest","All you need are some sticks, some paper, and some string. Refer to the plans, and you should have no problems.","Gather sticks in the Forest and string from the Lodge Attic.","Gather the kite parts",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Beach","I have filled up the gas, checked the oil and the engine. Everything! And I still cannot get it to start. Well, what do you know? It just needed a good strong pull. Thanks a lot. Is there anything I can help you with in return?","Fix the boat and borrow the air pump.","Pull the starter cord",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Plaza","Send help! Send soap! Oh! Thank you so much! I wish there was some way to repay you. Do you mind if I take this gum? Yeah sure, it is all yours. Be careful, though. It is really sticky.","Free the puffle bowlers and take the bubble gum.","Take the gum",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"Gadget Room","This deflated rubber duck might prove useful for installing the device at sea, though it has a hole you will have to patch. I also drafted up some plans for a kite that might work for the high-point tracker. The rest is up to you, Agent.","Build the kite and patch the rubber duck.","Assemble the gear",{3,0,5,0,{0,1,2,3,4,0,0,0},0,{"Sticks","Paper","String","Patch","Inflate"}}},
  {4,"Ski Hill","This sign post makes a great anchor.","Fly the kite from the Ski Hill and anchor the high tracker.","Launch the kite",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {5,"Mine Shack","I am fixing the troughs on the shack here. If you can climb up and help me figure them out, that would be mighty fine. You see that mine cart on its side? It took one too many back-flips on a corner, if you know what I mean. It could use some tender care and a welding torch.","Fix the troughs, weld the cart and ride down with the low tracker.","Fix the cart and ride",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {6,"Iceberg","The floating transmitter is working just fine.","Inflate the rubber duck and float the sea tracker.","Launch the duck",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {7,"Ski Lodge","Update me, Agent. Have you located Herbert? Affirmative. He is at a small campsite across the lake. If we can recover the phone, we can connect it to the Binoculars 3000 and set up a surveillance camera. Here are your orders, Agent: Find a way across the pond, recover your spy phone, and improvise a makeshift camera. Hide it and then return to HQ.","Use the Binoculars 3000, cross the pond and hide the camera in a tree.","Set up the camera",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {8,"HQ","It is unfortunate that he found the camera. But I think our mission has been successful. We will review the footage, and should Herbert try anything, we will be ready for him. Excellent work, Agent. We will be in contact soon.","Report back to G.","Report to G",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION10 = {
 10,7,10,923,300,"Waddle Squad",
 {"hq","beach","lighthouse","beakers","giftshop","dock","nightclub"},
 {"missions/10/hq.gpk","missions/10/beach.gpk","missions/10/lighthouse.gpk","missions/10/beakers.gpk","missions/10/giftshop.gpk","missions/10/dock.gpk","missions/10/nightclub.gpk"},
 {
  {0,"HQ","Welcome, Agent. I've called this group meeting because you'll all be working on a team for your next mission. Penguins in the Mine have made an amazing discovery. The mysterious golden puffle has been found. For now we've closed off the Night Club, and we'll be guarding it there until we announce it to the public.","Attend the team briefing.","Start the briefing",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","Hello, my fowl friends. It's a pleasure to see you again. I just wanted to give you a little heads up. I've cooked up quite a scheme, and it involves your precious little puffles. Oh yeah! Well there's no way you'll ever get the legendary golden puffle that's in the Night Club!","Hear Herbert out.","Answer Herbert",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","Greetings, agents. I am the leader of the PSA. My code name is The Director. Rookie's outburst was actually part of our plan. We didn't want Herbert to have any doubts that the golden puffle was real. The golden puffle is a ruse, a fake. But it makes for ideal Herbert bait.","Meet the Director and take command.","Accept the mission",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Beach","What a time for my jet pack to break down! I busted a fuel line, and all my fuel spilled. I need some emergency fuel ASAP. You can whip up emergency fuel with the right mixture of cream soda and hot sauce. I already have some hot sauce, but I still need some cream soda.","Fetch the cream soda barrel from the Lighthouse.","Go to the Lighthouse",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"Beaker table","Alright now, this mixture has to be done just right, or it'll blow the engine. I've already added the hot sauce. I just need you to divide the cream soda into 2 different containers, each holding 4 units.","Leave 4 units in the 8 and 4 in the 5.","Pour the soda",{3,0,7,0,{0,1,2,3,4,5,6,0},0,{"8 to 5","5 to 3","3 to 8","5 to 3","8 to 5","5 to 3","3 to 8"}}},
  {4,"Gift Shop","Anyways, I can't get the Magnet 3000 working. I've got the magnet here, and the little doo-dad's here, but it won't work. It needs power! G made a solar panel power unit for it, but I forgot to grab it! Can you grab it and install it?","Take the solar panel from HQ and wire it into the magnet.","Install the solar panel",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Solar panel","Cable","Magnet"}}},
  {5,"Dock","Phew! Another long day. I do have some rope. Would you like to use it? Alright, here you are.","Take the rope to the Night Club. Klutzy was spotted at the Dock.","Take the rope",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {6,"Night Club","This cage needs to be lifted for the trap to be set. Looks like another one of G's contraptions has broken. It'll need fixing before you can capture Herbert.","Attach the rope, pull the lever and repair the gearbox.","Repair the gearbox",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Attach rope","Pull lever","Repair gearbox"}}},
  {6,"Night Club","Oh no you don't! That golden puffle is MINE! The magnet isn't strong enough to hold him! I've got a clear view of the suspect! What are your orders? If I had more light that would give the magnet more power.","Cut the string, then aim the stage lights at the magnet.","Aim the lights",{3,0,2,0,{0,1,0,0,0,0,0,0},0,{"Cut string","Aim stage lights"}}},
  {0,"HQ","Great work, agents, on the successful capture of Suspect #1, code-named Herbert. Yes. I demand to make my one phone call. Sure! You can use my phone. ROOKIE, NO! HAHAHA! NICE TRY, AGENTS, BUT I WIN THIS ROUND! They're seeds. It's a package of seeds. What would Herbert want with these? It looks like we've got another mystery on our hands.","Debrief with G.","Finish mission",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData MISSION11 = {
 11,9,10,927,300,"The Veggie Villain",
 {"hq","giftshopoffice","skivillage","dvddevice","lighthouse","mineshack","maze","herbdesk","colorlock"},
 {"missions/11/hq.gpk","missions/11/giftshopoffice.gpk","missions/11/skivillage.gpk","missions/11/dvddevice.gpk","missions/11/lighthouse.gpk","missions/11/mineshack.gpk","missions/11/maze.gpk","missions/11/herbdesk.gpk","missions/11/colorlock.gpk"},
 {
  {0,"HQ","As far as I can tell, nothing. The seeds have been a dead end so far. The reason you're here is actually because a number of items are missing from the Gift Shop. Jet Pack Guy and Rookie are already on the scene. Your mission will be to join their investigation and see what you can find.","Report to the Gift Shop office.","Go to the Gift Shop",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {1,"Gift Shop office","As for the case, it's pretty straightforward. The Gift Shop computer is missing. Well, usually there's two. Oh, my! There's only one here! The other chair's missing! I thought those tracks looked suspiciously un-penguin-like. I'll put up some tape around the scene!","Search the office and bag the corn clue.","Examine the office",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","Agent! We have a Code Red emergency! As you can see, I've completely taken over the PSA main computer. And in mere moments I shall blow the cover of the PSA to all of Club Penguin. Every channel. Every frequency. First, head to the Ski Village and shut down Herbert's broadcast. We cannot let him compromise our identity.","Shut down the broadcast at the Ski Village.","Go to the Ski Village",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {2,"Ski Village","I've got an idea! No one can see the movie if they close their eyes! It's too bad we can't just put in another movie. You can probably put a movie into the projector device I found. Don't they sometimes show movies in the Lighthouse?","Find a movie disc for Rookie's projector.","Ask Rookie",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {4,"Lighthouse","No, I left it under a piano somewhere.","Take the disc from under the piano and clean off the dust.","Take the disc",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {3,"DVD device","That should stop Herbert's video feed. Great work, the signal seems to be coming from the Mine Shack!","Play the disc, then trace the signal.","Trace the signal",{2,0,4,0,{0,0,0,0,0,0,0,0},"RGBY",{0}}},
  {5,"Mine Shack","An enormous pile of popcorn! You can't cross the stream, you will need to find something to help you get across. There must be something you can use to get this ladder down.","Follow the popcorn trail through the tunnels to Herbert's camp.","Cross the stream",{3,0,3,0,{0,1,2,0,0,0,0,0},0,{"Cross the stream","Lower the ladder","Follow the popcorn"}}},
  {7,"Herbert's camp","And then he gave me his phone! He just gave it to me! I'm about to reveal your identity to the entire island! Oh dear! It looks like you've pulled the plug on my little computer take-over. You've foiled my plan again.","Pull the plug on Herbert's terminal.","Pull the plug",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
  {0,"HQ","Agents, we don't have much time. Try to find a way out of here while I try to disarm the machine! The door is locked tight, we can't get out this way!","Crack the colour lock on the Gadget Room door.","Open the colour lock",{2,0,4,0,{0,0,0,0,0,0,0,0},"YGOB",{0}}},
  {0,"HQ","Wow, ace! Nobody's ever opened this door without clearance before. PSA tech is down, but EPF's is fine! My teleporter works. Prepare to be moved out! Oh no! The HQ! It's been destroyed!","Evacuate with Dot.","Finish mission",{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}},
 },
};

static const MissionData *const MISSION_FLOW_DATA[]={&MISSION4,&MISSION5,&MISSION6,&MISSION7,&MISSION8,&MISSION9,&MISSION10,&MISSION11};
