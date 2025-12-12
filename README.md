# Cleanai-c in-dev 0.0.17

## What's this?
I'm the guy that made <a href="https://github.com/willmil11/cleanai">cleanai</a> which is basically javascript pytorch made from scratch with no machine learning librairies. Except I originally made that one as a python library then translated it to js for speed then added a cli arround it etc. It is very unclean and pretty slow, therefore I decided to make this version in c with better design choices.

## Who are you?
I am willmil11, a 15 year old french self taught dev.

## Why are you doing this?
I tought about how cleanai's codebase is pure hot steaming garbage and decided to remake it but in C.

## How long have you been working on this?
I've been working on the <a href="https://github.com/willmil11/cleanai">original cleanai repo</a> for almost 11 months (although realistically I stopped working on it since 6 months ago so more like 5 months). And I've been working on this repo you're on right now for about 4 months. (Note that this information is true today, dec. 12 2025 but will change in the future, that is how time works.)

## How to use?
<strong>Automatic way</strong>
<br>
Make sure you have the fish shell installed, then just:
```bash
fish compile.fish help
```
And normally the script will display an easy to understand help message, the script is actually super super simple to use so just run the help command like I showed you and you'll understand pretty much immediately.
<br>
<i>Note: script requires gcc and fish shell, obviously.</i>
<br>
<br>
<strong>Manual way</strong>
<br>
You can compile the code manually with
```bash
gcc -O3 -march=native -ffast-math cleanai.c -o cleanai -lm -pthread
```
(You need gcc installed. This code can only be compiled with gcc because it uses gcc only things like nested functions. You can still compile for windows tho because there are builds of gcc that work on windows. You can also cross compile if you remove "-march=native" from your command and use a cross compiler.)

## Version history
- in-dev 0.0.17: Better compile.fish.
- in-dev 0.0.16: Fixed token mask bugs, fixed many bugs including a few segfaults. Improved ui slightly also fixed vocabulary by adding <|unk|> token.
- in-dev 0.0.15: Added pretraining, dataset eta, a --config-init for easy config making and other improvements, mask bug from in-dev 0.0.14 still here tho.
- in-dev 0.0.14: many improvements, but the mask isn't having the indented effect for some reason, so i'm gonna fix that next update unless I procrastinate.
- in-dev 0.0.13: Training is sort of working but there are many bugs that I'm fixing.
- in-dev 0.0.12: Optimized code.
- in-dev 0.0.11: Small improvements.
- in-dev 0.0.10: Overall a lot of fixes, improved model architecture, new inference chat interface, better config, better vocabulary, etc...
- in-dev 0.0.9: I did some mining off camera ahh update 🙏😭
- in-dev 0.0.8: Updated readme.
- in-dev 0.0.7: Inference function is built, works and should be bugless.
- in-dev 0.0.6: Removed old vocabulary file.
- in-dev 0.0.5: Inference function is being built. I removed all the shared memory things (because I am gonna use another method) and reworked the vocabulary.
- in-dev 0.0.4: I made a few ml functions and added a save() function.
- in-dev 0.0.3: I added model loading, it is also loaded in shared memory.
- in-dev 0.0.2: I added model initialization, it is initialized in shared memory.
- in-dev 0.0.1: I already implemented argument parsing, config parsing and vocabulary parsing. Tokenizer from scratch is on the way.

# License
Click <a href="./LICENSE.md">here</a> to access the license.
