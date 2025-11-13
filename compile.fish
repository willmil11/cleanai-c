if test "$argv[1]" = "build=hyperspeed"
    echo "Compiling hyperspeed build..."
    echo "+ time gcc -Ofast -mavx2 -mfma -funroll-loops -ffast-math -funsafe-math-optimizations -fno-math-errno -fomit-frame-pointer -flto -fwhole-program -march=native cleanai.c -o cleanai -lm"
    time gcc -Ofast -march=native -mavx2 -mfma -funroll-loops -ffast-math -funsafe-math-optimizations -fno-math-errno -fomit-frame-pointer -flto -fwhole-program -march=native cleanai.c -o cleanai -lm
    echo "Compiled."
else
    if test "$argv[1]" = "build=normal"
        echo "Compiling normal build..."
        echo "+ time gcc -O3 -march=native -ffast-math cleanai.c -o cleanai -lm"
        time gcc -O3 -march=native -ffast-math cleanai.c -o cleanai -lm
        echo "Compiled."
    else
        if test "$argv[1]" = help
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
            echo "  - build=hyperspeed enables hyper agressive optimisations to go as fast as possible, which I recommend."
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
            echo "  - build=hyperspeed enables hyper agressive optimisations to go as fast as possible, which I recommend."
        end
    end
end
