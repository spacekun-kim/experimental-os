if [[ ! -d "../lib" || ! -d "../build" ]];then
	echo "dependent dir don\'t exist!"
	cwd=$(pwd)
	cwd=${cwd##*/}
	cwd=${cwd%/}
	if [[ $cwd != "command" ]];then
		echo -e "you\'d better in command dir\n"
	fi
	exit
fi

BIN="cat"
CFLAGS="-m32 -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIB="-I ../lib/ -I ../lib/user/ -I ../fs -I ../lib/kernel/ -I ../kernel/ -I ../device/ -I ../thread/ -I ../userprog/ -I ../shell/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/jyj/bochs/demo/hd60M.img"

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $OBJS start.o
gcc $CFLAGS $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

#if [[ -f $BIN ]];then
	dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
#fi
