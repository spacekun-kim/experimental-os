nasm -I ./loader/include/ -o ./build/MBR.bin ./loader/MBR.S && dd if=./build/MBR.bin of=./hd60M.img bs=512 count=1 seek=0 conv=notrunc
nasm -I ./loader/include/ -o ./build/loader.bin ./loader/loader.S && dd if=./build/loader.bin of=./hd60M.img bs=512 count=3 seek=2 conv=notrunc
nasm -f elf -o ./build/print.o ./lib/kernel/print.S
nasm -f elf -o ./build/kernel.o ./kernel/kernel.S

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -o build/timer.o device/timer.c
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o ./kernel/interrupt.c 
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -I device/ -c -fno-builtin -o build/init.o ./kernel/init.c 
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/main.o ./kernel/main.c 

ld -m elf_i386 -Ttext 0xc0001500 -e main -o build/kernel.bin build/main.o build/init.o build/interrupt.o build/print.o build/kernel.o build/timer.o
dd if=./build/kernel.bin of=./hd60M.img bs=512 count=200 seek=9 conv=notrunc
