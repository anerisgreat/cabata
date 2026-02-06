{ pkgs } :
let
  pythonWithPackage = (pkgs.python3.withPackages
    (python-pkgs: with python-pkgs;
      [ gtts ] ));

  firstNum = 0;
  lastNum = 60;
  nums = pkgs.lib.range firstNum lastNum;
  numAttrs = builtins.listToAttrs
    (map (n: { name = "num${toString n}"; value = toString n; }) nums);
  prompts = {
    round = "round";
    of = "of";
    youhave = "You have";
    towork = "to work";
    workfor = "work for";
    torest = "to rest";
    restfor = "rest for";
    hoursleft = "hours left";
    minutesleft = "minutes left";
    secondsleft = "seconds left";
    hours = "hours";
    minutes = "minutes";
    seconds = "seconds";
    done = "done!";
    paused = "Timer paused!";
    resumed = "Timer resumed!";
    started = "Timer started!";
    skrakaka = "skrakaka!";
    message001 = "Congrats, you survived another morning—now try not to waste the rest of the day.";
    message002 = "If you’re feeling lazy, just remember: at least you’re not as lazy as your future self.";
    message003 = "Great news! You still have time to procrastinate before the deadline hits.";
    message004 = "Your to‑do list is growing—so at least you have something to complain about.";
    message005 = "Remember, every tiny step forward is still a step away from total failure.";
    message006 = "If you can’t find motivation, at least you’ve mastered the art of avoidance.";
    message007 = "The glass is half empty, but hey, you finally noticed the emptiness.";
    message008 = "Another day, another chance to prove that you’re not completely hopeless.";
    message009 = "Don’t worry, the universe hates you just as much as you hate yourself—so you’re evenly matched.";
    message010 = "You’re one coffee away from pretending you’ve got this under control.";
    message011 = "If you think today’s a disaster, imagine how spectacular tomorrow could be—if you survive.";
    message012 = "Your potential is like a black hole: massive, dark, and probably swallowing everything.";
    message013 = "At least you’re not stuck in a perpetual loop of success—mediocrity iplay_soundplay_sounds more realistic.";
    message014 = "The only thing you’re guaranteed to improve is your ability to make excuses.";
    message015 = "Good thing you’re not perfect; otherwise, you’d have to actually try.";
    message016 = "Remember: the road to success is paved with a lot of missed turns and sighs.";
    message017 = "If you’re feeling down, just think of all the things you’ll never have to worry about—like ambition.";
    message018 = "Your effort may be minimal, but your disappointment will be impressively consistent.";
    message019 = "Celebrate small victories; they’re the only things that won’t betray you later.";
    message020 = "If you’re stuck, maybe it’s because you’re finally learning how to stay exactly where you are.";
    message021 = "You’re doing a fantastic job of proving that ‘trying’ isn’t a requirement for failure.";
    message022 = "The future looks bleak, but at least you’ll have plenty of time to stare at it.";
    message023 = "Your dreams are like Wi‑Fi—always out of reach, but you keep looking for the signal.";
    message024 = "If you can’t see the light, maybe you’re just used to the darkness by now.";
    message025 = "Remember, every setback is just a free lesson in how not to succeed.";
    message026 = "You’ve got the perfect excuse for today—tomorrow’s still a mystery.";
    message027 = "Your optimism is low, but that makes the rare moments of hope feel extra dramatic.";
    message028 = "If you’re aiming low, you’re already ahead of most people’s expectations.";
    message029 = "The best part about failure is that it’s guaranteed; at least you won’t be surprised.";
    message030 = "Don’t worry about the finish line—you’ll probably never get there anyway.";
    message031 = "Your ambition is like a cactus: prickly, dry, and best admired from a distance.";
    message032 = "If you can’t move forward, at least you’re consistent in staying put.";
    message033 = "The world’s not falling apart; it’s just taking a long coffee break—just like you.";
    message034 = "Your resilience is impressive—how else would you survive your own negativity?";
    message035 = "Every time you think you’ve peaked, reality reminds you it’s actually a plateau.";
    message036 = "You may not be a hero, but you’re definitely a cautionary tale.";
    message037 = "If you’re feeling stuck, congratulations—you’ve finally met your personal limit.";
    message038 = "Your plans are like sandcastles: beautiful until the tide of reality washes them away.";
    message039 = "At least you’ve mastered the skill of overthinking—nothing else seems to be working.";
    message040 = "If you’re aiming for greatness, remember that mediocrity has a comfy couch.";
    message041 = "Your future self will thank you for all the time you’re wasting today.";
    message042 = "Every small win is a reminder that you’re not completely incompetent.";
    message043 = "If you can’t find a spark, maybe you’re just supposed to be a burnt-out bulb.";
    message044 = "You’re doing great at turning potential into procrastination.";
    message045 = "Your optimism may be low, but at least you won’t be disappointed by lofty expectations.";
    message046 = "Remember, the only thing you’re guaranteed to lose is the will to try.";
    message047 = "If you’re feeling hopeless, just think of the endless possibilities for new disappointments.";
    message048 = "Your effort is like a Wi‑Fi signal—there, but barely detectable.";
    message049 = "You’re one step closer to being exactly where you said you’d never end up.";
    message050 = "If you’re afraid of failure, at least you’ve got a solid backup plan: not trying at all.";
    message051 = "Your ambition is a nice decorative wall art—looks good, but never actually used.";
    message052 = "Don’t worry about the mountain; you’re already at the bottom, and that’s a start.";
    message053 = "Your future is uncertain, but that’s just more room for creative excuses.";
    message054 = "If you keep going at this pace, you’ll definitely reach… somewhere, eventually.";
    message055 = "You’re mastering the fine art of doing nothing with a lot of flair.";
    message056 = "Every setback is a free reminder that you’re still human—flawed and sarcastic.";
    message057 = "If you think you’re failing, just remember: you’re consistently underachieving, which is a kind of consistency.";
    message058 = "Your goals are like distant stars—pretty to look at, but you’ll never get close enough to touch.";
    message059 = "Congratulations, you’ve turned indecision into a lifestyle.";
    message060 = "If you can’t find motivation, at least you’ve found a reason to stay comfortable.";
    message061 = "Your progress may be glacial, but at least it’s not completely stagnant.";
    message062 = "Remember, the darkest clouds make for the best dramatic lighting in your personal tragedy.";
    message063 = "You’ve got the perfect balance of cynicism and procrastination—truly a rare talent.";
    message064 = "If you’re feeling unproductive, just think of all the time you’ll have to regret it later.";
    message065 = "Your resilience is impressive; you keep bouncing back to the same spot over and over.";
    message066 = "The road ahead is full of potholes, but at least you won’t have to drive fast.";
    message067 = "If you’re stuck in a rut, consider it a personalized trench—exclusive to you.";
    message068 = "Your ambition may be low, but your capacity for sarcasm is off the charts.";
    message069 = "Every day you survive is a tiny victory over your own lack of enthusiasm.";
    message070 = "If you ever feel like giving up, remember: you’ve already mastered the art of giving in.";
    message071 = "Your future looks bleak, but that’s just the perfect backdrop for a dramatic comeback—maybe.";
    message072 = "You’re doing a fantastic job of turning potential energy into wasted effort.";
    message073 = "If you can’t see the silver lining, maybe the clouds are just too thick.";
    message074 = "Your optimism is on vacation, but at least you have a solid excuse for your mood.";
    message075 = "Remember, the only thing you’re guaranteed to improve is your ability to complain.";
    message076 = "If you’re feeling lost, congratulations—you’ve finally found the direction you never wanted.";
    message077 = "Your determination is like a flickering candle—visible, but hardly useful.";
    message078 = "Every small step forward is still a step away from total stagnation.";
    message079 = "If you’re doubting yourself, at least you’re consistent in your self‑sabotage.";
    message080 = "Your future may be uncertain, but your talent for sarcasm is crystal clear.";
    message081 = "Don’t worry about the finish line; the real challenge is staying awake until then.";
    message082 = "Your ability to overthink is impressive—who needs action anyway?";
    message083 = "If you think you’ve hit rock bottom, at least you’ve finally found a solid foundation.";
    message084 = "Your dreams are like distant fireworks—bright, fleeting, and completely out of reach.";
    message085 = "Remember, every failure is just a plot twist in the tragedy of your life.";
    message086 = "If you can’t get motivated, at least you’ve perfected the art of realistic expectations.";
    message087 = "Your progress may be slow, but at least it’s not nonexistent.";
    message088 = "You’re one step closer to becoming the cautionary tale you always wanted to avoid.";
    message089 = "If you’re feeling overwhelmed, just remember: the world isn’t getting any easier.";
    message090 = "Your ambition may be on the brink of collapse, but your sarcasm holds it together.";
    message091 = "Every setback is a reminder that you’re still in the game—just not winning.";
    message092 = "If you can’t find a spark, maybe you’re just meant to be a dimly lit nightlight.";
    message093 = "Your perseverance is admirable; you keep trying even when you know the outcome.";
    message094 = "Remember, the only thing certain in life is that you’ll eventually run out of excuses.";
    message095 = "If you’re stuck, at least you have plenty of time to contemplate how you got here.";
    message096 = "Your future looks grim, but that just means you won’t be disappointed by success.";
    message097 = "Every day you get up is a tiny triumph over your own apathy.";
    message098 = "If you think you’re failing, congratulations—you’ve finally met your own low standards.";
    message099 = "Your potential is like a locked door—maybe it’s better left that way.";
    message100 = "Don’t forget, even a broken clock is right twice a day—so there’s still a sliver of hope.";  } // numAttrs;
  commandLines = ''
mkdir -p wav-files
rm -f wav-files/*.wav
'' + builtins.concatStringsSep "\n"
  (builtins.attrValues
    (builtins.mapAttrs (key: value:
      "gtts-cli \"${value}\" --output - | ffmpeg -hide_banner -loglevel error -i - -ar 16000 -ac 1 -sample_fmt s16 -f wav wav-files/${key}.wav") prompts));
  gen-wav-files = pkgs.writeShellApplication {
    name = "gen-wav-files";
    runtimeInputs = with pkgs; [
      pythonWithPackage
      ffmpeg
    ];
    text = commandLines;
  };
in {
  type = "app";
  program = "${gen-wav-files}/bin/gen-wav-files";
}
