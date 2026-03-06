# Cleanai-c 1.2.1

## What's this?
This is a cli to easily with almost no setup, pre-train, train and use a transformer chatbot, see I'm the guy that made <a href="https://github.com/willmil11/cleanai">cleanai</a> which is basically javascript pytorch made from scratch with no machine learning libraries. Except I originally made that one as a python library then translated it to js for speed then added a cli around it etc. It is very unclean and pretty slow, therefore I decided to make this version in c with better design choices.

## Who are you?
I am willmil11, a 15 year old french self taught dev.

## Why are you doing this?
I thought about how cleanai's codebase is pure hot steaming garbage and decided to remake it but in C.

## How long have you been working on this?
I've been working on the <a href="https://github.com/willmil11/cleanai">original cleanai repo</a> for almost 13.5 months (although realistically I stopped working on it since 8.5 months ago so more like 5 months). And I've been working on this repo you're on right now for about 7 months. (Note that this information is true today, mar. 6 2026 but will change in the future, that is how time works.)

## How to install?
Make sure you have the fish shell, gcc, curl and git installed then just run
```bash
cd $(mktemp -d) #To not pollute your filesystem
curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish #To download the script
sudo fish install.fish install #To run the script with the install flag
```
<strong>Note:</strong> If you're on windows make sure to use wsl2, I dropped native windows support but wsl2 gives you linux on windows with almost native cpu speed and its from microsoft so it works well with windows.
<br>
<strong>Note 2:</strong> If you are paranoid about viruses just check out the script before actually running it.

## How to update
Same requirements as to install but run this instead:
```bash
cd $(mktemp -d) #To not pollute your filesystem
curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish #To download the script
sudo fish install.fish update #To run the script with the update flag
```

## How to uninstall
Same requirements as to install but run this instead:
```bash
cd $(mktemp -d) #To not pollute your filesystem
curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish #To download the script
sudo fish install.fish uninstall #To run the script with the uninstall flag
```

## How to use?
Literally just run
```bash
cleanai
```
And the help message should be intuitive enough to know how to use the tool. But the general flow is get data to pre-train and train then use the --init-config flag like the help message says to build your config then use that config to pre-train and train your model.

## Some basic knowledge
### Tokenization
Tokenization is the process of taking some data often text and breaking it into pieces, words called tokens the model can understand. Every input to the model is tokenized and the output also being in tokens is turned back into text.
### Pre-training
When creating a model you'll want to pre-train first, this means giving files of any type to the model to train the goal is not for the model to learn conversation but more like the general structure of language to make basically an autocomplete on steroids. You can have multiple pre-training datasets to split your data across.
### Training
This part, done after pre-training has for goal to make the model use the language skills taught by pre-training to have conversations and behavior and go from autocomplete on steroids to a smart conversationalist. However the files for this are recommended to be .json and must be structured like this:
```jsonc
[ //This is the entry list
    { //This is an entry
        "system_prompt": "This is the system prompt of the conversation entry",
        "turns": [ //This is the turn sets list of the entry
            {
                "person": "This is what the person would say.",
                "model": "And this is what we train the model to answer."
            },
            //Other turn sets here (last turn set must not have a "," after it)
        ]
    },
    //Other entries here (last entry must not have a "," after it)
]
```
You can have multiple dataset files to split your data across.

### Good instincts
If your loss is not really decreasing try to increase your learningRate by a little bit, but if it has been decreasing but now is not anymore try to decrease your learningRate by a bit you might need finer, more precise learning. If your loss reaches 0 it means your model has overfitted (learnt the data perfectly) which is generally bad because you generally want your model to think and not just repeat the dataset, the optimal loss is often between 1 and 2. If after a lot of epochs your loss doesn't decrease despite learningRate adjustments your model might just not have enough capacity, parameters to learn the data well enough, try to raise things like embeddingSize, heads, ffnGrowSize, layers, etc... Also for epoch number, I recommend choosing a very high number you'll probably never reach because you can always stop training in the checkpoint clis every epoch.

## Version history
- 1.2.1: Made the agc more loose for faster training.
- 1.2.0: Fixed bugs for training correctness involving RoPE, config parsing and other things. Tho the current approach for RoPE could be made more efficient but that's for next version.
- 1.1.7: Fixed numerical instability. Aka agc was added (credit to claude for this specific change)
- 1.1.6: Removed a few comments, I'm not even gonna bump up the last date that I worked on this because this update is so insignificant it barely even deserves to be mentioned here.
- 1.1.5: Updated README.
- 1.1.4: Fixed a single printf.
- 1.1.3: Changed nothing this is just to test if the update checking works.
- 1.1.2: Added automatically checking if an update is available.
- 1.1.1: Added --version flag to help output.
- 1.1.0: Updated install.fish to be able to update, check deps and auto install them if user agrees with package manager auto detect based on distro, better compile flags, added a blas version of cleanai, both are now compiled and installed by the script as cleanai-blas and cleanai-original and cleanai-blas is symlinked as cleanai so it is the default one as it is faster but they're the same except internal changes for blas. If I sum up way better performance thanks to blas and better install.fish, so now you have cleanai-original (no blas) and cleanai-blas (with blas) and the default cleanai command is now a symlink pointing to cleanai-blas (and they both behave the same except speed).
- 1.0.0: Changed compile.fish to install.fish, added better vocabulary file logic to make it work with real installs, removed windows support because people can just use wsl2 and windows native is a nightmare and updated README.
- in-dev 0.0.20: Fixed bugs.
- in-dev 0.0.19: Improved eta display.
- in-dev 0.0.18: Fixed a few memory leaks and improved some printfs.
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
