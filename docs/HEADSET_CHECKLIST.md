# SH3 Quest VR: what only the headset can show (LIST 38)

For the first headset run of the APK (after the one build pass). Each line: what to do, what should happen. Anything
wrong shows in sh3.log / sh3-crash.log (the launcher's Logs tab, Download/SH3QuestVR): note the time and the place.

## Start
- [ ] Install, open: the launcher on a panel, What's new once. Game files: choose the "SILENT HILL 3" folder copied to
      Download: the copy runs with the panel closed; the check says all files are there and names the sh3.exe release.
- [ ] 0.1.4 (owners 90, 91, 93: "wrong version", the European release): with another copy's sh3.exe (a European or
      Asian one, the disc's protected exe, a no-CD exe, a PC Fix exe) the check passes, naming "the 2003-09-03 build
      (its code changed ...)", and the game plays; sh3.log's first lines: "the 2003-09-03 build's data, all 163 pages
      the same" (or "N of 163 data pages differ"), the code section's data and the import table as found. A game
      folder one level too deep (game/SILENT HILL 3/...) is moved up ("moved up" on the check). An exe the check
      is not sure of: "Try it anyway" plays it.
- [ ] Right after the check: the Features popup, once (dynamic cutscenes first, the Cutscenes row's choices); the Features
      button beside What's new shows it again.
- [ ] Play in VR: the game starts in the headset (no flat window), the title on the panel, the pointer works on it.
- [ ] Play flat: the game on a big screen, the controllers as a pad.
- [ ] 0.1.5 (a player's video: black after ~5 s of play): play 10 minutes, open and close the VR menu, the
      inventory and the map, turn the left wrist to the watch and away again many times: never a black or frozen
      picture, never a 2-second stop. sh3.log has no "a frame took" line in plain play (only at loads).
- [ ] 0.1.5, an update from 0.1.4: sh3.log "defaults ... folded": Smoother frames, ghost hands and Heather's shadow
      off, a Quest 3 or 3S at Render resolution 1.0x; 120 Hz. Show performance: steady at 60 game frames a second
      in the park and the mall.
- [ ] 0.1.6, an update from 0.1.5 with Performance options changed (72 Hz, 1.2x, foveation High, say): the launcher's
      Options, Performance shows every row at its default (120 Hz, 1.0x, Dynamic resolution on, Smoother frames off,
      Medium, boost on, Sharpening off, Show performance off, Game threads three cores); sh3.log (game started on the PC
      rig without the launcher) "sh3vr.ini from before 0.1.6 ... every Performance option at its default once ...
      changed: ...". Change one afterwards: it stays after quitting and starting again. Show performance at 120 Hz in
      the park and the mall: the game's frames a second as 0.1.5's (about 55-95), few displays missed; turning the head and
      walking look even (no judder or double edges).
- [ ] 0.1.6 (owner 115, a player: "Enemies only become visible when they're right on top of you"): the opening
      nightmare's park, first person: the dogs and other monsters show as soon as they are in view at a distance
      (in the fog and the dark as the game draws them), not only when they reach Heather; sh3.log "a model the game's
      camera leaves out ... drawn: the head sees it" and every 20 s "N model draws ... drawn from the head's view".
      Third person: as the game draws it.
- [ ] 0.1.6 Sprint toggle (on): a left-stick click runs; she keeps running until the stick is clicked again or let go
      half a second; sh3.log "sprint on / off (Sprint toggle: ...)". Off (Options, Comfort): runs only while held in.
- [ ] 0.1.6 (owner 111, the arms twisting two-handed): a gun in both hands, walk back and to the sides while looking
      ahead: her body keeps facing where you look ("walking back ... her body keeps facing the head"), no 180-degree
      turn, the arms never cross through her, the second grip held (no "second grip ... past her arm's reach").
- [ ] 0.1.6 the reload hint (Manual reload, the first reloads): a small level label just over where you look; look down
      at the hands: it follows into view, level, readable, not over the hands; small head moves leave it still.
- [ ] 0.1.6 "Health and stamina on your wrist" (View): off, no bars on the watch and the game's HUD in first person
      (if on in Extra options); on, the bars again.
- [ ] 0.1.7 (a player: "I could not open the second green gate at the start ... after passing the first gate I can't
      see my hands anymore"): the park's entrance gate and the circular hall's far door each open with A while looking
      at them, standing a little to the side or with her body turned away; sh3.log "the action button (pass 0) found
      event N where the player looks" when her own facing missed. In the dark hall, the hands held low or at rest are
      in sight (the chest flashlight's spill), looking ahead and glancing down; with the flashlight off they go dark
      with the room.
- [ ] 0.1.7 every A or hand use of a door, item or examine ticks and buzzes once; A pressed while walking: she stops
      where she is and does not walk on through the message.
- [ ] 0.1.7 (a player's video: stuck to the dead dog): the hall's dog holds her: the purple break-free bar under the
      gaze (smooth, lit by each shake); knife it during the hold: the hold breaks; kill it: she is free at once and
      never pulled back to where it died. "Break-free bar when grabbed" off: no bar.
- [ ] 0.1.7 "Melee damage" (Hands and weapons, 200%): the hall's dog down in about six real knife slashes.
- [ ] 0.1.7 (owner 123): walking, strafing and turning with a gun held low or in both hands: no bump, no ding, the
      weapon never held back by nothing ("held at a surface" only at real walls). Under a room lamp, her shadow on the
      floor moves with her body as you move it (no walking legs under you, no dark blob at the eyes). No "Heather's
      shadow" row in Options or the VR menu.
- [ ] 0.1.7 two-hand grip spot (VR menu, Hands and weapons): Adjust, then take the second grip where you like on the
      maul or the shotgun: a buzz and the use sound, the spot kept for that weapon only (sh3quest_weapons.ini); "Reset
      the weapon in your hand" brings back that one's natural spot, "Reset every weapon" all of them.
- [ ] 0.1.7 grips (owner 129): every gun (handgun, shotgun, submachine gun, stun gun, flamethrower) from the eyes and
      turned in the hand: her fist closed round the grip, the trigger finger at the trigger and following yours; with
      the second hand on, the handgun's support hand round the front of the fist, the submachine gun's under its front
      with the thumb along its side (not over the top). The knife, pipe, katana, maul and beam saber in the game's own
      hands, nothing through the handle.
- [ ] 0.1.7 (a player: the Flat version a black screen with sound): Play flat: the game on the big screen in the
      headset, the controllers as its pad, the VR menu works.
- [ ] 0.1.7 lean or step forward in your room (through a door, say), stand back and push the stick at once: the view
      comes back to her head within a second or so as she walks (no hair or collar in view, no knock on her hair), and
      the extra drift is comfortable (sh3.log "walking with the stick, the head N m from her: taken up as she walks").
- [ ] 0.1.7 "Melee damage" at 400%: a fast knife slash on a dog takes off about 270 (sh3.log "took 270.0 damage from
      her (melee: 900% ...)"), never the game's bare 30.
- [ ] 0.1.7 (session 28, the rig's vr332/vr336): raise both hands before your face, the controllers pointing ahead,
      then look 45 degrees left and right and turn your wrists: her elbows hang below the hands, never come up into
      your eyes (no faded forearm before the view); sh3.log "her left elbow kept clear of the eyes" says when it acted.
- [ ] 0.1.7 press A where nothing is: sh3.log "the action button (pass 0) found nothing along her facing ... nor where
      the player looks", with the ways and places it tried.
- [ ] 0.1.7 (owner 140: knife stabs spammed left her "only walking", monsters not attacking): walk into the hall's
      dog with the stick held while stabbing it again and again; let it grab her and knock her down. Each time she must
      go on to the struggle (the bar, shakes or the stick free her) and get up, then walk, swing and use A again, and
      the dog must attack again. sh3.log "her hit reaction (the game's movement state N): VR's walk and turn wait" and
      "her hit reaction over after N frames" (never hundreds of frames held).
- [ ] 0.1.7 hints (owner 143-144), from a new install (or "Show every hint again"): as play starts, the walking line
      then the VR menu line, each a few seconds in view, level, over where you look, fading in and out; near a door or
      a thing to look at, "Something to use here"; an item, "An item here"; the dog's grab, "Shake both controllers
      hard to break free" at once (before any other line), gone when she is free; the first gun and first melee weapon
      drawn, their lines; the flashlight; health under half, her watch and the supplies; each gun's first reload, its
      steps. Then never again (a new run too: sh3quest_tips.ini). "Hints" off: none, the reload steps too. VR menu,
      Hints page: every line readable, none cut; "Show every hint again" brings them back.
- [ ] 0.1.8 the trigger finger (owner 146, 151), the handgun, shotgun and submachine gun: rest your finger on the
      controller's trigger: her index lies on the gun's own trigger, its pad on the blade; lift it off: her finger lies
      straight along the frame, clear of the guard. Pull slowly: the gun's trigger swings back about its pin as far as
      you pull and her finger closes with it, its pad on it the whole way, never apart and never through it; all the
      way, it fires. sh3.log once a touch: "the right hand's trigger finger on the trigger (curl ..., spread ...);
      pulled a quarter at a time it closes to ..., its pad on the blade the whole way". The flamethrower and the stun
      gun (no trigger in their models): the index closes round the grip with the others.
- [ ] 0.1.8 every weapon gripped (owner 146-148): each gun and melee weapon from the eyes, looking down and close
      (turn it in your hand): her knuckles and palm out of it, her palm against it, every finger closed round it and
      stopping on it (none through it), her thumb lying along the frame or round the grip, never standing out to the
      side; the knife, steel pipe, katana, maul and beam saber in her own hand too. A gun aims where 0.1.7's aimed (the
      guide dot and the shots).
- [ ] 0.1.8 the second hand: on every two-hand weapon the other hand wraps the support grip, the pump or the handle;
      on the handgun it wraps your main hand's fingers, never through them. Pump the shotgun: the pump goes back with
      your hand, to the receiver (0.1.7 slid it forward). Rack the empty handgun: its trigger stays with the gun.
- [ ] 0.1.8 Hold weapons anywhere (Hands and weapons, off to start): on, grip a gun's barrel or slide or a pipe's
      shaft with the other hand: it takes hold there, wrapped round that part; off, it takes each weapon's two-hand
      grip. A Two-hand grip spot moved 2 cm or more off the natural grip wraps the part there too.
- [ ] 0.1.8 Swing strength needed (Hands and weapons, Normal to start): moving a melee weapon or a gun about, or
      walking with it, never attacks; a firm swing does. Light attacks as 0.1.7 did; Hard asks a full swing.
- [ ] 0.1.8 updating from 0.1.7 with a weapon moved and a two-hand grip spot set: sh3.log "sh3quest_weapons.ini was
      0.1.7's: the guns' and stun gun's moves and turns (N set) and the two-hand grip spots (N set) put back for
      0.1.8's fitted hands; sizes, aims, guide dots and the melee weapons' kept", once; a melee weapon's move and a size
      changed stay.
- [ ] 0.1.8 Left-handed (Hands and weapons): all of the above mirrored, the left hand on each grip and trigger.
- [ ] 0.1.8 the beam saber: lit, the beam leaves the emitter along the hilt's line, held, swung, forward and up and
      across, as on 0.1.7; held in both hands, the other hand on the hilt beside the main hand, its mirror image.
- [ ] 0.1.8 a key kept until its door opens (a player's report): with a key in the inventory, come up to its door,
      reach a hand toward it, look at it, let a hint show: the key stays in the inventory; press A at the door: it opens
      and only then is the key used. The bookstore's key and one other door's key.
- [ ] 0.1.8 standing through an event (a player's report): hold the stick (and once with Sprint toggled on) and use
      something with a message (a door, an item, a note): she stands while the message shows and walks again with the
      stick as soon as it is closed; the Sprint run ended.
- [ ] 0.1.8 the item taken: as you take an item, it turns in its box on the message's panel, once, never double in
      the eyes.

## View and comfort
- [ ] New Game: the opening movie plays on the big cinema screen in front of you (sharp, 4:3, no stutter), with its
  sound in step with the picture at the game's music level; turning your head leaves the screen where it is; the
  game's cancel or start button skips it and the opening goes on. The endings and the other movies alike when reached.
- [ ] New Game, the opening: the cutscene around you, in stereo, head tracked; subtitles readable. Dynamic cutscenes
      (on): through Heather's eyes while the scene shows her from 1.5 to 20 m (the opening's park: her eyes as she
      walks it), the game's camera when it shows something else or her face up close; no switching back and forth
      (each view holds at least 1.5 s); sh3.log "dynamic cutscenes: ...". A scene ending in her eyes goes on into play
      in first person with her body under you, facing where you face (sh3.log "first person from a scene into play").
      Never in her eyes with something right before them: the opening's bridge walk, where she stands at the ride's
      fence, stays at the game's camera (sh3.log "the world N cm before her eyes"). The VR menu's View page:
      "Cutscenes" reads "Dynamic (her eyes or the camera)" (0.1.3, owner 87); "At the camera": always the game's camera.
- [ ] 0.1.3 (owner 84: "this weird fuzz over the screen"): no grain at all by default, standing still in a lit place:
      neither the Noise Effect's speckle nor the fine grey fuzz over everything lit (the game's second grain, sh3.log
      "the game's second grain ... drawn as its average"); the picture as bright as 0.1.2's (the darkest parts a shade darker). "Film grain" in the VR
      menu puts both back.
- [ ] 0.1.3, the defaults (owner 84): a 0.1.2 install updated keeps its saves; its options take 0.1.3's new defaults once
      (sh3.log "sh3vr.ini from before 0.1.3"): drifting fog off, smooth turning, no sharpening, film grain, smooth
      edges or vignette, the picture at 100%; a change made afterwards stays.
- [ ] 0.1.2, the fog (owner 79, a player's "weird overlay that looks like fog"): in the opening's alley and the
      circular hall the game's drifting fog hangs far off in the world (about 10 m), the same in both eyes, never as
      patches in front of the face; turn the head: it stays where it is (with "Drifting fog" on: off by default since 0.1.3).
- [ ] 0.1.5, lens flares left out (as the PC VR mod): no starburst, rings or streaks anywhere in VR (the opening's
      wall lamps, the park); the flat screen keeps them. No depth-of-field blur in cutscenes, no shaking double
      picture. In the park in first person: no coloured orbs in the air, the lamps' bulbs lit; turn the head:
      nothing stays fixed in the view. Third person (VR menu, View, "First person" off): the glows sit on their
      lamps. Fire every gun: the flash at the muzzle in both eyes, the case thrown from the slide, no streak
      beside the gun. The handgun's flash at the front of its slide, rising with the kick (owner 109).
- [ ] 0.1.5, the stomp (owner 110): knock a monster down, stand by it with the trigger released and press A: she
      stomps it (or kicks it, by the monster), in first and in third person. With the trigger held A does not stomp
      and the trigger still fires.
- [ ] 0.1.5, Dynamic cutscenes: when a scene goes to Heather's eyes you face where her face looks; in her eyes no
      hair or inside of her head in view; at the scene's camera and in third person she is drawn whole.
- [ ] 0.1.5, both sticks clicked together: the view recentred on Heather, a sound and a short buzz; one stick's
      click alone still runs (left) or searches (right). Head in a wall for 2 s: the view back at Heather.
- [ ] 0.1.2, the picture (View): Brightness, Contrast, Color saturation and Darker edges change the picture at once
      (100 / 100 / 100 / 0 % the game's own look); Smooth edges (off by default since 0.1.3) takes the jaggies off.
- [ ] Her shadow (LIST 56): with the flashlight on, raise a hand into its beam: the beam's spot on the floor or wall
      ahead darkens in the hand's shape and moves with it (fingers, a thumbs up, a fist); look down: her legs' shadows
      at her feet, rounded, following your steps; a gun, the pipe or the katana held out casts its own; her arms and
      hands are never darkened by her own shadow (sh3.log "Heather's shadow in the game's shadow pass: N triangles
      (... of her body, ... of the weapon, ... of a part held)"). "Heather's shadow" (the VR menu's View page, and
      the launcher's Options; off by default since 0.1.5: turn it on for this check) off: none at once, the game's own shadows as before; on again: back at once.
- [ ] The game's own character shadows: in a room where sh3.log says "the game's own character shadows are on here",
      her shadow from the game's room light keeps her animation's pose (the game's shadow model), while the
      flashlight's shadow follows your body; look for doubled or wrong shadows there and say where (the room).
- [ ] First person in the mall: eyes at Heather's height, the world life size (doors, counters); no doubled or
      swimming picture when moving the head; nothing missing at the edges of the eyes.
- [ ] Walk in the room: Heather follows; a wall stops her (the view never goes through a wall without darkening).
- [ ] Left stick walks where the head looks (the default; the left hand when chosen); the right stick turns smoothly
      (0.1.3's default, at its set speed however the frame rate goes; snap 45° when chosen); no vignette unless chosen.
- [ ] Hold the left menu button 1 s, anywhere (play, a cutscene, a menu): the VR menu, with its sound; in play the
      game pauses under it. A short press closes it (the game's pause menu under it), another goes back to play. A
      short press in play pauses, as before. The menu's View page, "First person" off: third person (the game's
      camera, in stereo); on: first person. Every row changes, and the launcher shows the change afterwards.
- [ ] A place where the game's camera matters (a fixed-camera room, a chase): third person comes and goes by itself.
- [ ] Performance: 120 Hz (the start; forced once by 0.1.6) with Show performance on (Smoother frames off; boost, foveation Medium;
      every headset at Render resolution 1.0x since 0.1.5); both eyes are drawn in one pass (multiview: VR does not
      start without it). Dynamic resolution (on): Show performance's "3D" share drops below 100% only while frames come
      late (a heavy room) and climbs back after 5 s of calm; the world a little softer meanwhile, text, menus and her
      hands as sharp as ever; no flicker or black frame at a change (sh3.log "dynamic resolution: the 3D pictures at").
- [ ] 0.1.4, Smoother frames (AppSW; off by default since 0.1.5: turn it on for this check; the task "make Smoother frames (AppSW) work"): sh3.log
      "[openxr] smoother frames (Application SpaceWarp): on, motion W x H", then in play "smoother frames: motion
      given" and "[vr-perf] ... smoother frames on N% of them" with N well above 0 in play (0 on the title before
      the first play). Walk and turn smoothly in the alley and the mall: smooth, no wobble or smearing at the edges of walls,
      doors or Heather's hands and weapon (they get no motion within 1 m); a camera cut or a snap turn shows no
      smear. Show performance: steady. Turn it off (VR menu, Performance, next start): each frame shown twice
      again, as 0.1.3. An options file from 0.1.4 gets it off once (0.1.5).

## Hands and weapons
- [ ] Hands at the controllers; fingers follow the triggers, grips and thumbs; they stop on a wall or a counter.
- [ ] A hand pushed into a wall stays at the wall, a faint ghost where the controller is.
- [ ] The extras (after a clear: beam saber, flamethrower, gold and silver pipes, unlimited submachine gun; session
      13): each in the hand at the controller, held the way it is used (a blade or pipe out of the thumb side, a gun's
      barrel along your pointing), never left where her animated hand would be; the unlimited submachine gun takes
      both hands like the submachine gun. sh3.log names each ("gold pipe", "flamethrower"...).
- [ ] Two hands on every two-hand weapon (shotgun, both submachine guns, handgun, pipes, katana, maul, beam saber,
      flamethrower; session 13): grip with the other hand near the second grip: the hand glides onto it in about a
      tenth of a second and the weapon turns to the new line smoothly, never a jump; let go: the hand glides back to
      the controller. The submachine gun: the left palm under the receiver's front, fingers round it, no fingers in
      the gun. Hold it far out at full stretch: the gun comes back toward her (at most 20 cm) so her hand stays on
      it, never her hand left at the arm's end beside the gun (sh3.log "held back toward her").
- [ ] A door, a drawer or a thing to look at opened by the free hand: a hand holding a weapon never takes (use the
      other hand, or holster first); the light tick when the hand comes onto it, the game's own action on the grip.
      Session 14: reaching toward it is enough wherever the action button would work from where you stand (up to
      about a metre ahead of her toward the hand); sh3.log "the left hand can use event N", then "used event N".
      Session 15 (sim, vr222): the opening's entrance door (event 5, the large door about 7 m from the start,
      ahead and to the left) opened by the left hand held ahead and closed, and the same door back from the circular hall
      (event 4): on the headset, check the light tick and the door's sound with the hand at it.
- [ ] Hands on surfaces (session 8): the palm rests flat on a table, a wall or the floor and slides along it; the
      fingers lie on it, never in it; at a table's or seat's edge they wrap down over it; a soft tap sound and buzz.
- [ ] Every hand action has a sound: a grip, a draw from a holster or the pouch, a magazine out and in, the slide,
      a hand on a surface, the flashlight's switch.
- [ ] The flashlight on her chest: the beam goes where you look; your hand held in it is lit where it is in the beam
      (its side away from the lamp dark), never a lit forearm beside a flat grey hand. Crouch and lean: the beam
      leaves her chest where it is drawn (it rides her body down; sh3.log "the chest lamp rides her chest as drawn"),
      and no shade of her own body sits in the beam's middle with the hands out to the sides (any weapon, vr188).
- [ ] Her watch (left wrist): turn the back of the wrist to you and look at it: two bars over the watch fade in, her
      health on top (yellow, red under a quarter; under half it pulses as the game's Life Display does) and her stamina
      under it (green; run a while and it drains, stand and it fills); turn the wrist away or look off: they fade out.
      Not shown in menus or cutscenes. With the game's Life Gauge and Life Display on (Extra options): in first person
      no bars at the view's top left and no red cross (the watch has them); third person (VR menu, View, "First person" off):
      the game's normal HUD, its two bars at the view's top left (about 22 degrees left, 16 up, easy to read, following
      the view gently) and the cross on the panel, no watch; back to first person: the top left empty, the watch again.
      "Health and stamina on your wrist" (View) off: the normal HUD in first person too.
- [ ] Shake to break free: let a monster knock her down or grab her, then shake both controllers hard side to side:
      each shake buzzes with a rustle, she gets up sooner than when waiting (sh3.log "VR: she is knocked down or held"
      and "she got free after ... s (N shakes counted ...)"); the buttons pressed fast still work.
- [ ] Blocking: hold a weapon or a forearm up between you and a monster as it hits: less damage (a buzz and a knock
      in that hand; sh3.log "a hit on her ...: guarded by ..."); a hit from behind or the side is not blocked.
- [ ] Handgun: in the hand at the right size and grip (sh3quest_weapons.ini for fine tuning); the red dot where it
      points; shots hit where it points; recoil and haptics.
- [ ] Shotgun, submachine gun: the off hand on the foregrip aims with both hands.
- [ ] Knife, steel pipe, katana, maul: a real swing attacks and hits where the weapon went; a lazy wave does nothing;
      the heavy ones hold Heather still while they swing.
- [ ] Melee with a monster near (session 14, owner 65): the hit lands the moment the blade, pipe or fist meets it, not
      when her animation would; it staggers or falls the way the weapon was moving (a swing from the right knocks it
      to the left); one hit a swing; a hard knock in that hand on impact. sh3.log: "hit ... at contact ... its way" and
      "took N damage from her (reaction R)". A punch lands at half the knife's damage, every punch that lands reacts.
- [ ] The beam saber (session 14, owner 61): pull the trigger and hold: the beam lights from the hilt in your hand,
      along the blade, never across the view by her hip; swing with it lit: it hits at contact; let go: it goes out.
      Session 15 (sim vr217-219: the blade follows the hilt up, across and in a swing): in the headset, also check
      that the blade stays on the hilt during a fast swing (no lag of a frame), that its green light falls on the
      hilt, your hand's surroundings and the floor near the blade (not by her hip), and that the ignition glow sits
      on the hilt.
- [ ] Empty hands: a hard jab punches (sh3.log: "melee attack id ... learned" before the first punch lands needs the
      knife or pipe used once); a gun swung hard clubs.
- [ ] Holsters (0.1.6, owner 122): every gun on the main-hand hip, every melee weapon over the main-hand shoulder
      (reach over it): grip there draws the one last put there (sh3.log "drawn from the holster"), letting go there
      puts it away; drawing one puts the held one in its own holster. "Gun on the hip, melee on the shoulder" off: the
      gun over the shoulder and melee at the hip, as 0.1.5. Belt pouch: supplies. Chest pocket: the flashlight. Easy to
      reach, no false draws when the hand passes the face or the belt.
- [ ] Radius options (0.1.6, owner 122): "Weapon holster radius" (18 cm) and "Ammo pouch radius" (14 cm, was 10):
      the ammo pouch found by feel without looking; at 30 cm both still pick the nearest place (the gun's holster and
      the supplies pouch do not steal a magazine grab); at 10 / 6 cm only a hand right on them.
- [ ] Spatial sound (headphones or the Quest's speakers): a monster, a door or the radio heard off to one side; turn
      your head and the sound stays where it is in the world (it moves to the other ear when you turn past it), and
      behind you it is a little softer and duller. sh3.log: "spatial sound: the game's listener is its camera's view".
- [ ] The radio's static in the hands (LIST 23, "Extra vibration" on): a monster nearing, the radio crackles and both
      hands feel a crackle that gets denser and harder as the static grows; none in cutscenes or menus (sh3.log "the
      radio's voice N at L").
- [ ] Looking down at the belt: the pouches and the hip holster where they are with the head level (they used to sink
      with the eyes).
- [ ] Manual reload, handgun: empty, its slide stays back; the right stick down drops the magazine; the off hand at the
      ammo pouch (the front of the belt) grips a new one; pushed up into the bottom of the grip it seats with a buzz and
      the reload sound (sh3.log "the hands reloaded item 10"); the slide gripped on top, pulled back (seen moving) and let
      go: it fires, the slide kicking back on each shot. The submachine gun the same with the knob on top at the back;
      the stun gun's battery needs no cocking.
- [ ] Shotgun: shells one at a time from the pouch into the port under the gun; from empty, the pump back and forward
      (seen moving) before it fires.
- [ ] No free ammo: the release with no spare keeps the magazine in ("No spare ammo"); the inventory's counts match.
      The steps show through the first reload of each gun, once (0.1.7: "Hints"); with the left controller lost the game
      reloads by itself.
- [ ] Reach and grip at an item: Heather picks it up; at a door: it opens.

## Screens
- [ ] Inventory, map, memos, menus: each opens whole on a screen in front of you (fading in, the world dark round it),
      stays put while you point, comes round after you turn over 35 degrees away for half a second, fades out on B.
- [ ] Grip Robbie the Rabbit in the first alley: "A rabbit doll... No, it's a costume." stands by him, not on a
      screen; with the button it shows ahead of you. Opening scene subtitles low in the view, readable.
- [ ] Ghost hands (off by default since 0.1.5: turn them on for this check): a controller held past her arm's reach: no ghost hand. A hand pushed into a wall: a faint ghost where it is.
- [ ] Inventory, map (zoom on the right grip), memos, the save screen, options, a puzzle screen: the beam's dot moves
      the game's cursor; the trigger chooses; B goes back. The inventory's tabs (Maps, Memos, Options): the first
      pull moves the selection to the tab, the second opens it (the PC game's own two clicks; vr187, vr189). On the
      OPTIONS screen the row under the dot lights as it moves (the game's own hover, vr195) and one pull chooses it
      (Exit closes it); its page arrows (vr189: pointing at one and pulling opened the highlighted Button Config
      instead): the left stick turns the pages; drag a slider (Brightness, the volumes) if the game lets the mouse;
      say if pointing feels wrong anywhere. The hand ticks on entering each thing a
      pull can choose (the game's own click zones) and the dot grows a little there; hold the beam still on an entry
      for 5 s, then pull: it still chooses (the game used to hide its cursor after 3 s; sh3.log "a pull at the
      game's cursor ... over click zone").
- [ ] The VR menu with the beam: left half of a value steps down, right half up.

## Records
- [ ] After a run, the launcher's Logs tab shows sh3.log; Share the report sends one text file.
- [ ] Force a crash only if one happens by itself: sh3-crash.log names the frame, the area, Heather and VR's state.
