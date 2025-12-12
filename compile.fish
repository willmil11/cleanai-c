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

    echo "+ time gcc $flags cleanai.c -o cleanai -lm"
    time gcc $flags cleanai.c -o cleanai -lm
    echo "Compiled."

else if test "$build_type" = "build=normal"
    echo "Compiling normal build..."
    echo "+ time gcc -O3 -march=native -ffast-math cleanai.c -o cleanai -lm"
    time gcc -O3 -march=native -ffast-math cleanai.c -o cleanai -lm
    echo "Compiled."

else if test "$build_type" = help
    echo ---------------------------
    echo "|           Help          |"
    echo ---------------------------
    echo "Valid usage of this script:"
    echo "fish compile.fish build=normal"
    echo "                  build=hyperspeed"
    echo "                  help"
    echo ""
    echo "Additional info:"
    echo "  - build=normal makes a stable simple build."
    echo "  - build=hyperspeed enables hyper aggressive platform-specific optimisations."

else
    echo ------------------------------------
    echo "|           Invalid usage          |"
    echo ------------------------------------
    echo "[Error] Invalid argument '$argv[1]'"
    echo "Valid usage of this script:"
    echo "fish compile.fish build=normal"
    echo "                  build=hyperspeed"
    echo "                  help"
    echo ""
    echo "Additional info:"
    echo "  - build=normal makes a stable simple build."
    echo "  - build=hyperspeed enables hyper aggressive platform-specific optimisations."
end
