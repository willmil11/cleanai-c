#!/usr/bin/fish

if test (id -u) -ne 0
    echo "You need to run this script as root, use sudo."
    exit 1
end

if not test "$argv[1]" = uninstall
    if not test "$argv[1]" = ""
        echo ------------------------
        echo "|         Help         |"
        echo ------------------------
        echo "Usage:"
        echo "  fish install.fish           -- to install."
        echo "  fish install.fish uninstall -- to uninstall."
        exit 1
    end
end

if not isatty stdin
    echo "No stdin, redownloading and respawning..."
    set temp_script (mktemp)
    curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o "$temp_script"
    set status_store "$status"
    if test "$status_store" = 127
        echo "Curl not found, exiting..."
        exit 1
    else
        if test "$status_store" != 0
            echo "Curl error ($status_store exit code), exiting..."
            exit 1
        end
    end
    echo "Downloading, spawning..."
    fish $temp_script $argv
    set exit_code $status
    rm $temp_script
    exit $exit_code
end

function compile
    set gcc_command gcc
    set build_type "$argv[1]"

    if test "$build_type" = "build=hyperspeed"
        echo "Compiling hyperspeed build..."

        set -l arch (uname -m)
        set -l os (uname -s)

        # Base flags for all platforms
        set -l flags -Ofast -funroll-loops -ffast-math -funsafe-math-optimizations -fno-math-errno -fomit-frame-pointer -flto -fwhole-program

        switch $arch
            case x86_64 i686
                # x86/x64 specific
                set -a flags -march=native -mavx2 -mfma -msse4.2
                if test "$os" = Linux
                    # Linux x86 can use these safely
                    set -a flags -fno-plt -fno-semantic-interposition
                end
            case aarch64 arm64
                # ARM64 (Apple Silicon, ARM servers, your phone)
                set -a flags -mcpu=native
                if test "$os" = Darwin
                    # macOS ARM specific
                    set -a flags -mtune=native
                else
                    # Linux ARM (like your phone)
                    set -a flags -mtune=native -moutline-atomics
                end
            case armv7l armv8l
                # 32-bit ARM
                set -a flags -mcpu=native -mfpu=neon-vfpv4 -mfloat-abi=hard
            case '*'
                # Fallback for unknown architectures
                set -a flags -march=native
        end

        # Link-time optimization threads
        set -a flags -flto=auto

        echo "+ time $gcc_command $flags cleanai.c -o cleanai -lm"
        time $gcc_command $flags cleanai.c -o cleanai -lm
        set status_store "$status"

        if test "$status_store" = 127
            echo "Gcc is not installed, please install gcc."
            return 1
        else
            if test "$status_store" = 1
                if $gcc_command --version | grep -q clang
                    echo "Gcc failed to compile because clang is pretending to be gcc, input real gcc command to compile or 'exit' to exit."
                    read -P "Real gcc command (anything or 'exit') " prompt_ans
                    if test "$prompt_ans" = exit
                        echo "Exiting..."
                        exit 0
                    end
                    set gcc_command "$prompt_ans"
                    compile build=hyperspeed
                    return "$status"
                end
                echo "Gcc failed to compile. Falling back to normal build..."
                compile build=normal
                return "$status"
            end
        end

        echo "Compiled."

    else if test "$build_type" = "build=normal"
        echo "Compiling normal build..."
        echo "+ time $gcc_command -O3 -march=native -ffast-math cleanai.c -o cleanai -lm"
        time $gcc_command -O3 -march=native -ffast-math cleanai.c -o cleanai -lm
        set status_store "$status"
        if test "$status_store" = 127
            echo "Gcc is not installed, please install gcc."
            return 1
        else
            if test "$status_store" = 1
                if $gcc_command --version | grep -q clang
                    echo "Gcc failed to compile because clang is pretending to be gcc, input real gcc command to compile or 'exit' to exit."
                    read -P "Real gcc command (anything or 'exit') " prompt_ans
                    if test "$prompt_ans" = exit
                        echo "Exiting..."
                        exit 0
                    end
                    set gcc_command "$prompt_ans"
                    compile build=normal
                    return "$status"
                end

                echo "Gcc failed to compile."
                return 1
            end
        end
        echo "Compiled."
    end
    return 0
end

if test "$argv[1]" = uninstall
    if test -d /usr/share/cleanai
        echo "+ rm -rf /usr/share/cleanai"
        rm -rf /usr/share/cleanai
    end
    if test -f /usr/bin/cleanai
        echo "+ rm /usr/bin/cleanai"
        rm /usr/bin/cleanai
    end
    echo "Uninstalled cleanai."
    exit 0
end

if not test -f cleanai.c
    echo "cleanai.c not found."
    while true
        read -P "Do you wish to clone the repo? (y/n) " prompt_ans
        if test "$prompt_ans" = y
            echo "+ time git clone https://github.com/willmil11/cleanai-c.git"
            time git clone https://github.com/willmil11/cleanai-c.git
            set status_store "$status"
            if test "$status_store" = 127
                echo "Git not found, please install git."
                exit 1
            else
                if test "$status_store" = 1
                    echo "Failed to clone repo."
                    exit 1
                end
            end
            echo "Cloned the repo."
            echo "+ cd cleanai-c"
            cd cleanai-c
            echo "Continuing..."
            break
        else
            if test "$prompt_ans" = n
                echo "Cannot continue, exiting..."
                exit 1
            else
                echo "Invalid input, try again."
                continue
            end
        end
    end
end
if not test -f vocabulary.json
    echo "vocabulary.json not found."
    while true
        read -P "Do you wish to clone the repo? (y/n) " prompt_ans
        if test "$prompt_ans" = y
            echo "+ time git clone https://github.com/willmil11/cleanai-c.git"
            time git clone https://github.com/willmil11/cleanai-c.git
            set status_store "$status"
            if test "$status_store" = 127
                echo "Git not found, please install git."
                exit 1
            else
                if test "$status_store" = 1
                    echo "Failed to clone repo."
                    exit 1
                end
            end
            echo "Cloned the repo."
            echo "+ cd cleanai-c"
            cd cleanai-c
            echo "Continuing..."
            break
        else
            if test "$prompt_ans" = n
                echo "Cannot continue, exiting..."
                exit 1
            else
                echo "Invalid input, try again."
                continue
            end
        end
    end
end

if not compile build=hyperspeed
    echo "Failed to compile, exiting..."
    exit 1
end

echo "+ mkdir -p /usr/share/cleanai/"
mkdir -p /usr/share/cleanai/
echo "+ cp vocabulary.json /usr/share/cleanai/vocabulary.json"
cp vocabulary.json /usr/share/cleanai/vocabulary.json
echo "+ cp cleanai /usr/bin/cleanai"
cp cleanai /usr/bin/cleanai

echo "Installed cleanai."
