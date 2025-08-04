gcc -o ./build/main main.c arena.c -g 

./build/main 2> profile.heap

chmod +x profile.heap

pprof --web ./build/main profile.heap 2> /dev/null
