# Cleanai-c in-dev 0.0.2

## What's this?
See I'm the guy that made <a href="https://github.com/willmil11/cleanai">cleanai</a> which is basically javascript pytorch made from scratch with no machine learning librairies. Except I originally made that one as a python librairy then translated it to js for speed then added a cli arround it etc. It is very unclean and pretty slow, therefore I decided to make this version in c with better design choices.

## Who are you?
I am willmil11, a 15 year old french self taught dev.

## Why are you doing this?
I've discovered C arround 3 months ago, since then I've toyed with it a lot, made a small os and stuff and I loved it, so I tought about cleanai and decided to remake it but in C :)

## How long have you been working on this?
I've been working on the <a href="https://github.com/willmil11/cleanai">original cleanai repo</a> since a very long time, however I've only been working on this one since 2 weeks or so.

## How to use?
I will add a guide link here very soon. You can already compile the code with
```bash
gcc -O3 -march=native cleanai.c -o cleanai
```
(You need gcc installed. This code can only be compiled with gcc because it uses gcc only things like nested functions. You can still compile for windows tho because there are builds of gcc that work on windows. You can also cross compile if you remove "-march=native" from your command and use a cross compiler.)

## Version history
- in-dev 0.0.2: I added model initialization, it is initialized in shared memory.
- in-dev 0.0.1: I already implemented argument parsing, config parsing and vocabulary parsing. Tokenizer from scratch is on the way.
