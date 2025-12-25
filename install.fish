#!/usr/bin/fish

if test (id -u) -ne 0
    echo "You need to run this script as root, use sudo."
    exit 1
end

if not test "$argv[1]" = uninstall
    if not test "$argv[1]" = ""
        if not test "$argv[1]" = update
            if not test "$argv[1]" = install
                echo ------------------------
                echo "|         Help         |"
                echo ------------------------
                echo "Usage:"
                echo "  fish install.fish           -- to install."
                echo "  fish install.fish install   -- also to install."
                echo "  fish install.fish update    -- to update."
                echo "  fish install.fish uninstall -- to uninstall."
                echo "  fish install.fish help      -- to show this message."
                exit 1
            else
                set $argv[1] ""
            end
        end
    end
end

if not isatty stdin
    echo "No stdin, this probably means you piped the script to fish, please run this instead:"
    echo "  curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish"
    echo "  fish install.fish"
    echo ""
    echo "Also if you don't want to pollute your filesystem with this file run this first:"
    echo "  cd \$(mktemp -d)"
    echo "This will put you in a unique directory generated in /tmp/"
    echo "Exiting..."
    exit 1
end

function compile
    set gcc_command gcc
    echo "Compiling $argv[1]."
    set -l arch (uname -m)
    set -l os (uname -s)
    # Base flags for all platforms
    set -l flags -Ofast -funroll-loops -fomit-frame-pointer -flto=auto -fwhole-program -fno-stack-protector -fgraphite-identity -floop-nest-optimize
    switch $arch
        case x86_64 i686
            # x86/x64 specific
            set -a flags -march=native
            if test "$os" = Linux
                # Linux x86 can use these safely
                set -a flags -fno-plt -fno-semantic-interposition
            end
        case aarch64 arm64
            # ARM64 (Apple Silicon, ARM servers, your phone)
            set -a flags -mcpu=native
            if test "$os" = Darwin
                # macOS ARM specific - add any macOS-specific flags here if needed
            else
                # Linux ARM (like your phone)
                set -a flags -moutline-atomics
            end
        case armv7l armv8l
            # 32-bit ARM
            set -a flags -mcpu=native -mfpu=neon-vfpv4 -mfloat-abi=hard
        case '*'
            # Fallback for unknown architectures
            set -a flags -march=native
    end
    if test "$argv[1]" = cleanai-original
        echo "+ time $gcc_command $flags cleanai.c -o cleanai-original -lm -lpthread"
        time $gcc_command $flags cleanai.c -o cleanai-original -lm -lpthread
    else
        if test "$argv[1]" = cleanai-blas
            echo "+ time $gcc_command $flags cleanai_blas.c -o cleanai-blas -lm -lpthread -lopenblas"
            time $gcc_command $flags cleanai_blas.c -o cleanai-blas -lm -lpthread -lopenblas
        else
            echo "+ time $gcc_command $flags cleanai.c -o cleanai-original -lm -lpthread"
            time $gcc_command $flags cleanai.c -o cleanai-original -lm -lpthread
        end
    end
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
                compile $argv[1]
                return "$status"
            end
            echo "Gcc failed to compile."
            return 1
        end
    end
    echo "Compiled $argv[1]."
    return 0
end

if test "$argv[1]" = uninstall
    if test -d /usr/share/cleanai
        echo "+ rm -rf /usr/share/cleanai"
        rm -rf /usr/share/cleanai
    end
    if test -f /usr/bin/cleanai-original
        echo "+ rm /usr/bin/cleanai-original"
        rm /usr/bin/cleanai-original
    end
    if test -f /usr/bin/cleanai-blas
        echo "+ rm /usr/bin/cleanai-blas"
        rm /usr/bin/cleanai-blas
    end
    if test -h /usr/bin/cleanai; or test -f /usr/bin/cleanai
        echo "+ rm /usr/bin/cleanai"
        rm /usr/bin/cleanai
    end
    echo "Uninstalled cleanai."
    exit 0
end

function checkforopenblas
    set testcfile "$(mktemp)"
    mv $testcfile "$testcfile.c"
    set testcfile "$testcfile.c"
    echo "int main(){ return 0; }" >"$testcfile"
    set testcfilecompiled "$(mktemp)"
    gcc "$testcfile" -o "$testcfilecompiled" -lopenblas &>/dev/null
    if test "$status" != 0
        rm "$testcfile"
        rm "$testcfilecompiled" &>/dev/null
        return 1
    else
        rm "$testcfile"
        rm "$testcfilecompiled" &>/dev/null
        return 0
    end
end

function install_missing_deps
    echo "Checking dependencies..."

    set -l missing_deps

    if not command -v gcc &>/dev/null
        echo "✗ gcc not installed"
        set missing_deps $missing_deps gcc
    else
        echo "✓ gcc already installed"
    end

    if not command -v curl &>/dev/null
        echo "✗ curl not installed"
        set missing_deps $missing_deps curl
    else
        echo "✓ curl already installed"
    end

    if not command -v git &>/dev/null
        echo "✗ git not installed"
        set missing_deps $missing_deps git
    else
        echo "✓ git already installed"
    end

    if not checkforopenblas
        echo "✗ openblas not installed"
        set missing_deps $missing_deps openblas
    else
        echo "✓ openblas already installed"
    end

    if test (count $missing_deps) -eq 0
        echo "All dependencies already installed."
        return 0
    end

    echo "Missing dependencies: $missing_deps"

    read -P "Auto install missing deps? (y/n) " prompt_ans
    while test "$prompt_ans" != y; and test "$prompt_ans" != n
        echo "Invalid input."
        read -P "Auto install missing deps? (y/n) " prompt_ans
    end

    if test "$prompt_ans" = n
        echo "Skipping dependency installation."
        return 1
    end

    echo "Installing missing dependencies..."

    if command -v apt &>/dev/null
        echo "Detected Debian/Ubuntu-based system"
        set -l apt_packages
        for dep in $missing_deps
            if test "$dep" = openblas
                set apt_packages $apt_packages libopenblas-dev
            else
                set apt_packages $apt_packages $dep
            end
        end
        echo "+ apt update"
        apt update
        if test "$status" != 0
            echo "Failed to update package lists."
            return 1
        end
        echo "+ apt install -y $apt_packages"
        apt install -y $apt_packages
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else if command -v pacman &>/dev/null
        echo "Detected Arch-based system"
        set -l pacman_packages
        for dep in $missing_deps
            set pacman_packages $pacman_packages $dep
        end
        echo "+ pacman -Sy --noconfirm $pacman_packages"
        pacman -Sy --noconfirm $pacman_packages
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else if command -v dnf &>/dev/null
        echo "Detected Fedora/RHEL-based system"
        set -l dnf_packages
        for dep in $missing_deps
            if test "$dep" = openblas
                set dnf_packages $dnf_packages openblas-devel
            else
                set dnf_packages $dnf_packages $dep
            end
        end
        echo "+ dnf install -y $dnf_packages"
        dnf install -y $dnf_packages
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else if command -v apk &>/dev/null
        echo "Detected Alpine-based system"
        set -l apk_packages
        for dep in $missing_deps
            if test "$dep" = openblas
                set apk_packages $apk_packages openblas-dev
            else if test "$dep" = gcc
                set apk_packages $apk_packages gcc musl-dev
            else
                set apk_packages $apk_packages $dep
            end
        end
        echo "+ apk update"
        apk update
        if test "$status" != 0
            echo "Failed to update package lists."
            return 1
        end
        echo "+ apk add $apk_packages"
        apk add $apk_packages
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else if command -v zypper &>/dev/null
        echo "Detected openSUSE-based system"
        set -l zypper_packages
        for dep in $missing_deps
            if test "$dep" = openblas
                set zypper_packages $zypper_packages openblas-devel
            else
                set zypper_packages $zypper_packages $dep
            end
        end
        echo "+ zypper install -y $zypper_packages"
        zypper install -y $zypper_packages
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else if command -v brew &>/dev/null
        echo "Detected macOS with Homebrew"
        echo "+ brew install $missing_deps"
        brew install $missing_deps
        if test "$status" != 0
            echo "Failed to install dependencies."
            return 1
        end
    else
        echo "Unknown package manager. Please manually install: $missing_deps"
        return 1
    end

    echo "Dependencies installed successfully."
    return 0
end

if test "$argv[3]" != already_checked_deps_for_u
    install_missing_deps
end

set old_pwd "$(pwd)"
if test "$argv[1]" = update
    if test -f "$(realpath $(status filename))"
        echo "Updating (uninstalling and re-installing)..."
        fish "$(realpath $(status filename))" uninstall
        fish "$(realpath $(status filename))" install pad already_checked_deps_for_u
        if test "$status" != 0
            echo "Failed to update cleanai."
            exit 1
        end
        echo "Updated cleanai."
    else
        echo "Didn't find self, re-downloading self in a temporary directory..."
        cd $(mktemp -d)
        curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish
        echo "Re-downloaded self in a temporary directory."
        echo "Updating (uninstalling and re-installing)..."
        fish install.fish uninstall
        fish install.fish install pad already_checked_deps_for_u
        if test "$status" != 0
            echo "Failed to update cleanai."
            cd "$old_pwd"
            exit 1
        end
        echo "Updated cleanai."
    end
    cd "$old_pwd"
    exit 0
end

if test -f /usr/bin/cleanai-original; or test -f /usr/bin/cleanai-blas; or test -f /usr/bin/cleanai; or test -d /usr/share/cleanai/
    echo "Cleanai is already installed on this machine, did you mean to update?"
    read -P "Update (y/n) " prompt_ans
    while test "$prompt_ans" != n; and test "$prompt_ans" != y
        echo "Invalid input."
        read -P "Update (y/n) " prompt_ans
    end

    if test "$prompt_ans" = n
        echo "Exiting..."
        exit 0
    else
        if test "$prompt_ans" = y
            if test -f "$(realpath $(status filename))"
                echo "Updating (uninstalling and re-installing)..."
                fish "$(realpath $(status filename))" uninstall
                fish "$(realpath $(status filename))" install pad already_checked_deps_for_u
                if test "$status" != 0
                    echo "Failed to update cleanai."
                    exit 1
                end
                echo "Updated cleanai."
            else
                echo "Didn't find self, re-downloading self in a temporary directory..."
                cd $(mktemp -d)
                curl https://raw.githubusercontent.com/willmil11/cleanai-c/refs/heads/main/install.fish -o install.fish
                echo "Re-downloaded self in a temporary directory."
                echo "Updating (uninstalling and re-installing)..."
                fish install.fish uninstall
                fish install.fish install pad already_checked_deps_for_u
                if test "$status" != 0
                    echo "Failed to update cleanai."
                    cd "$old_pwd"
                    exit 1
                end
                echo "Updated cleanai."
            end
            cd "$old_pwd"
            exit 0
        end
    end
end

set old_pwd "$(pwd)"

if not test -f cleanai.c
    echo "cleanai.c not found."
    while true
        read -P "Do you wish to clone the repo? (y/n) " prompt_ans
        if test "$prompt_ans" = y
            set temp_dir (mktemp -d)
            cd "$temp_dir"
            echo "+ time git clone https://github.com/willmil11/cleanai-c.git"
            time git clone https://github.com/willmil11/cleanai-c.git
            set status_store "$status"
            if test "$status_store" = 127
                echo "Git not found, please install git."
                cd "$old_pwd"
                exit 1
            else
                if test "$status_store" = 1
                    echo "Failed to clone repo."
                    cd "$old_pwd"
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
                cd "$old_pwd"
                exit 1
            else
                echo "Invalid input, try again."
                continue
            end
        end
    end
end
if not test -f cleanai_blas.c
    echo "cleanai_blas.c not found."
    while true
        read -P "Do you wish to clone the repo? (y/n) " prompt_ans
        if test "$prompt_ans" = y
            set temp_dir (mktemp -d)
            cd "$temp_dir"
            echo "+ time git clone https://github.com/willmil11/cleanai-c.git"
            time git clone https://github.com/willmil11/cleanai-c.git
            set status_store "$status"
            if test "$status_store" = 127
                echo "Git not found, please install git."
                cd "$old_pwd"
                exit 1
            else
                if test "$status_store" = 1
                    echo "Failed to clone repo."
                    cd "$old_pwd"
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
                cd "$old_pwd"
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
            set temp_dir (mktemp -d)
            cd "$temp_dir"
            echo "+ time git clone https://github.com/willmil11/cleanai-c.git"
            time git clone https://github.com/willmil11/cleanai-c.git
            set status_store "$status"
            if test "$status_store" = 127
                echo "Git not found, please install git."
                cd "$old_pwd"
                exit 1
            else
                if test "$status_store" = 1
                    echo "Failed to clone repo."
                    cd "$old_pwd"
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
                cd "$old_pwd"
                exit 1
            else
                echo "Invalid input, try again."
                continue
            end
        end
    end
end

set succeeded_len 0
set total_builds 2
if not compile cleanai-original
    echo "Failed to compile cleanai-original, moving onto cleanai-blas."
else
    set succeeded $succeeded cleanai-original
    set succeeded_len $(math "$succeeded_len" + 1)
end

if not compile cleanai-blas
    echo "Failed to compile cleanai-blas."
    if test "$succeeded_len" != 0
        echo "$succeeded_len/$total_builds builds succeeded ($succeeded)."
    else
        echo "$succeeded_len/$total_builds builds succeeded, exiting with code 1..."
        exit 1
    end
else
    set succeeded $succeeded cleanai-blas
    set succeeded_len $(math "$succeeded_len" + 1)
    echo "$succeeded_len/$total_builds builds succeeded ($succeeded)."
end

echo "+ mkdir -p /usr/share/cleanai/"
mkdir -p /usr/share/cleanai/
echo "+ cp vocabulary.json /usr/share/cleanai/vocabulary.json"
cp vocabulary.json /usr/share/cleanai/vocabulary.json
set success_cleanai_original false
set success_cleanai_blas false
for success in $succeeded
    if test "$success" = cleanai-original
        echo "+ cp cleanai-original /usr/bin/cleanai-original"
        cp cleanai-original /usr/bin/cleanai-original
        set success_cleanai_original true
    end
    if test "$success" = cleanai-blas
        echo "+ cp cleanai-blas /usr/bin/cleanai-blas"
        cp cleanai-blas /usr/bin/cleanai-blas
        set success_cleanai_blas true
    end
    if $success_cleanai_original; and $success_cleanai_blas
        break
    end
end
if $success_cleanai_blas
    echo "+ ln -s /usr/bin/cleanai-blas /usr/bin/cleanai"
    ln -s /usr/bin/cleanai-blas /usr/bin/cleanai
else
    if $success_cleanai_original
        echo "+ ln -s /usr/bin/cleanai-original /usr/bin/cleanai"
        ln -s /usr/bin/cleanai-original /usr/bin/cleanai
    else
        echo "There has been a bitflip or physics is broken today, try again later."
    end
end

echo "Installed cleanai."
exit 0
