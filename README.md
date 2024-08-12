This is a demo to show some coding skills in UE5/C++. Most of the work is done in PracticeProjectCharacter.h/.cpp.

Currently Implemented:
Grappling Hook - left click mouse button to connect to a point + move towards it, click again to release.
Requires movement, still not perfect as the math was giving me a lot of unexpected behaviors so I'm still smoothing out some bumps.

Press C to switch between third and first person camera - I do not reccomend trying the grappling hook in third person, I haven't yet accomodated for that functionality

Crouch - Hold ctrl to crouch. In the player's anim blueprint you will see I added nodes for a crouch animation + transitioning between that animation and normal idle/walking.
