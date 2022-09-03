BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/ -I shell/ -I network/
ASFLAGS = -f elf
CFLAGS = -m32 -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/list.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/builtin_cmd.o $(BUILD_DIR)/hash.o $(BUILD_DIR)/cmd_hash.o $(BUILD_DIR)/exec.o $(BUILD_DIR)/assert.o $(BUILD_DIR)/wait_exit.o $(BUILD_DIR)/pipe.o $(BUILD_DIR)/ne2000.o $(BUILD_DIR)/ethernet.o $(BUILD_DIR)/CRC32.o $(BUILD_DIR)/network.o $(BUILD_DIR)/transport.o $(BUILD_DIR)/socket.o $(BUILD_DIR)/ksoftirqd.o $(BUILD_DIR)/queue.o $(BUILD_DIR)/application.o $(BUILD_DIR)/signal.o

##########C##########
$(BUILD_DIR)/main.o: kernel/main.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/init.o: kernel/init.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/interrupt.o: kernel/interrupt.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/timer.o: device/timer.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/debug.o: kernel/debug.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/string.o: lib/string.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/memory.o: kernel/memory.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/thread.o: thread/thread.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/list.o: lib/kernel/list.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/sync.o: thread/sync.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/console.o: device/console.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/keyboard.o: device/keyboard.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ioqueue.o: device/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/tss.o: userprog/tss.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/process.o: userprog/process.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/stdio.o: lib/stdio.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/stdio-kernel.o: lib/kernel/stdio-kernel.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/syscall.o: lib/user/syscall.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/syscall-init.o: userprog/syscall-init.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ide.o: device/ide.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/fs.o: fs/fs.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/inode.o: fs/inode.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/file.o: fs/file.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/dir.o: fs/dir.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/fork.o: userprog/fork.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/shell.o: shell/shell.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/builtin_cmd.o: shell/builtin_cmd.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/hash.o: lib/kernel/hash.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/cmd_hash.o: shell/cmd_hash.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/exec.o: userprog/exec.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/assert.o: lib/user/assert.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/wait_exit.o: userprog/wait_exit.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/pipe.o: shell/pipe.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/CRC32.o: network/CRC32.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/socket.o: network/socket.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ne2000.o: network/ne2000.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ethernet.o: network/ethernet.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/network.o: network/network.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/transport.o: network/transport.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ksoftirqd.o: thread/ksoftirqd.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/queue.o: lib/kernel/queue.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/application.o: network/application.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/signal.o: kernel/signal.c
	$(CC) $(CFLAGS) $< -o $@


########ASM##########
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

#######LINK##########
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY : mkdir hd clean all

mkdir:
	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR); fi

hd:

#	dd if=./build/MBR.bin of=./hd60M.img bs=512 count=1 seek=0 conv=notrunc
#	dd if=./build/loader.bin of=./hd60M.img bs=512 count=3 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=./hd60M.img bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: $(BUILD_DIR)/kernel.bin

all: mkdir build hd
