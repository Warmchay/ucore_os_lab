# 1 实验准备

os需要通过某种机制加载并运行它，在这里我们通过更加简单的软件--boot loader来完成这些工作，为此，我么需要一个能够切换到x86的保护模式并显示字符的bootloader，为启动ucore作准备。lab1提供了一个非常小的bootloader和ucore OS，整个bootloader执行代码小于512字节，这样才能放到硬盘的主引导扇区中。

# 2 实验内容

lab1中包含一个bootloader和一个OS，这个bootloader可以切换到x86保护模式，能够读磁盘并加载ELF执行文件格式，并显示字符。

## 练习一：make生成执行文件

### 1.1 ucore.img镜像文件是如何生成的（需要详细的解释Makefile中每一条相关命令和命令参数的含义，以及说明命令导致的结果）

1. GCC编译选项

```bash
	-g 增加gdb调试信息
	-Wall 显示警告信息
	-O2 优化处理（有0， 1， 2， 3， 0为不优化）
	-fno-builtin 只接受以“__”开头的内建函数
	-ggdb 让gcc为gdb生成更丰富的调试信息
	-m32 编译32位程序
	-gstabs 这个选项以stabs格式生成调试信息，但是不包括gdb调试信息
	-nostdinc 不在标准系统目录中搜索头文件，只在-l指定的目录中搜索
	-fstack-protector-all 启用堆栈保护，为所有函数插入保护代码
	-E 仅做预处理，不尽心编译、汇编和链接
	-x c 指明使用的语言为C语言

LDD Flags
	-nostdlib	不链接系统标准启动文件和标准库文件，只把指定的文件传递给连接器
	-m elf\_i386 使用elf_i386模拟器
	-N 把text和data节设置为可读写，同时取消数据节的页对齐，取消对共享库的链接
	-e func	以符号func的位置作为程序开始运行的位置
	-Ttext addr	连接时将初始地址重定向为addr
```

2. 编译bootloader

> 用来加载Kernel 
>
> 先把bootasm.S, bootmain.c 编译成目标文件
>
> 再使用连接器链接到一起，使用start符号作入口，并且指定text段在程序中的绝对位置为0x7c00，0x7c00为os一开始加载的地址

```bash
//bootasm.o
+ cc boot/bootasm.S
gcc -Iboot/ -fno-builtin  -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o

//生成bootmain.o
+ cc boot/bootmain.c
gcc -Iboot/ -fno-builtin  -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootmain.c -o obj/boot/bootmain.o

//ld bin/bootblock
ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o

'obj/bootblock.out' size: 468 bytes
build 512 bytes boot sector: 'bin/bootblock' success!
```

3. **编译Kernel**

> 操作系统本身
>
> 先把.c文件和.S汇编文件生成目标文件，之后使用链接起生成Kernel

```bash
+ cc kern/init/init.c
gcc -Ikern/init/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o
+ cc kern/libs/readline.c
gcc -Ikern/libs/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/libs/readline.c -o obj/kern/libs/readline.o
+ cc kern/libs/stdio.c
gcc -Ikern/libs/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/libs/stdio.c -o obj/kern/libs/stdio.o
+ cc kern/debug/kdebug.c
gcc -Ikern/debug/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/kdebug.c -o obj/kern/debug/kdebug.o
+ cc kern/debug/kmonitor.c
gcc -Ikern/debug/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/kmonitor.c -o obj/kern/debug/kmonitor.o
+ cc kern/debug/panic.c
gcc -Ikern/debug/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/panic.c -o obj/kern/debug/panic.o
+ cc kern/driver/clock.c
gcc -Ikern/driver/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/clock.c -o obj/kern/driver/clock.o
+ cc kern/driver/console.c
gcc -Ikern/driver/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/console.c -o obj/kern/driver/console.o
+ cc kern/driver/intr.c
gcc -Ikern/driver/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/intr.c -o obj/kern/driver/intr.o
+ cc kern/driver/picirq.c
gcc -Ikern/driver/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/picirq.c -o obj/kern/driver/picirq.o
+ cc kern/trap/trap.c
gcc -Ikern/trap/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/trap.c -o obj/kern/trap/trap.o
+ cc kern/trap/trapentry.S
gcc -Ikern/trap/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/trapentry.S -o obj/kern/trap/trapentry.o
+ cc kern/trap/vectors.S
gcc -Ikern/trap/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/vectors.S -o obj/kern/trap/vectors.o
+ cc kern/mm/pmm.c
gcc -Ikern/mm/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/mm/pmm.c -o obj/kern/mm/pmm.o
+ cc libs/printfmt.c
gcc -Ilibs/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/  -c libs/printfmt.c -o obj/libs/printfmt.o
+ cc libs/string.c
gcc -Ilibs/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/  -c libs/string.c -o obj/libs/string.o

+ ld bin/kernel
ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/readline.o obj/kern/libs/stdio.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/debug/panic.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/intr.o obj/kern/driver/picirq.o obj/kern/trap/trap.o obj/kern/trap/trapentry.o obj/kern/trap/vectors.o obj/kern/mm/pmm.o  obj/libs/printfmt.o obj/libs/string.o
```

4. **编译sign**

> 用于生成一个复合规范的硬盘主引导扇区

```bash
+ cc tools/sign.c
gcc -Itools/ -g -Wall -O2 -c tools/sign.c -o obj/sign/tools/sign.o
gcc -g -Wall -O2 obj/sign/tools/sign.o -o bin/sign
```

**在这里也有第二问的答案，我们来看看sign.c**

```bash
less tools/sign.c
```

<img src="/Users/zoriswang/Library/Application Support/typora-user-images/image-20210405213234119.png" alt="image-20210405213234119" style="zoom:50%;" />

第一个扇区为bootloader的位置，通过0x7c00指向这，前510个字节全部为0（用于初始化），后面第511和512个字节必须为：

```c
buf[510] = 0x55;
buf[511] = 0xAA;
```

否则硬盘启动会失败

5. **生成ucore.img**

- dd - 转换和拷贝文件

```bash
if	表示输入文件，如果不指定if，默认会从stdin中读取输入
of	表示输出文件，如果不指定of，默认会将stdout作为默认输出
bs	代表字节为单位的块大小
count 	表示被复制的块数
/dev/zero	是一个字符设备，会不断返回0值字节(\0)
conv=notrunc	输入文件的时候，源文件不会被截断
seek=blocks		从输出文件开头跳过blocks（512字节）个块后再开始复制
```

- **过程：** 生成一个空的软盘镜像，然后把bootloader以不截断的方式填充到开始的块中，然后kernel会跳过bootloader所在的块，再填充

```bash
dd if=/dev/zero of=bin/ucore.img count=10000
dd if=bin/bootblock of=bin/ucore.img conv=notrunc
dd if=bin/kernel of=bin/ucore.img seek=1 conv=notrunc
```

### 1.2 主引导扇区的特征是什么

上面提过的，一个磁盘主引导扇区只有512个字节，并且`buf[510] = 0x55`, `buf[511] = 0xAA`

## 练习2：使用qemu执行并调试lab1中的软件

1. 从CPU加电后执行的第一条指令开始，单步跟踪BIOS的执行
2. 在初始化位置0x7c00设置实地址断点，测试断点正常
3. 在调用qemu时增加`-d `

### 2.1 qemu-system-i386

```bash
-hda file	硬盘选项
-parallel dev	重定向虚拟并口到主机设备，最多可虚拟3个并口
-serial dev	重定向虚拟串口到主机设备
	vc: 虚拟控制台
	pty: 仅仅linux有效，虚拟tty（一个虚拟伪终端会被立刻分配）
	none: 没有设备被分配
  null: 无效设备
-S	启动的时候不直接从CPU启动，需要在窗口中按c来继续
-s	shorthand for -gdb tcp::1234，打开端口1234，供gdb来调试
```

### 2.2 gdb

```bash
-x	从文件中执行gdb命令
-q	不打印介绍和版权信息
-tui	将终端屏幕分成原文本窗口和控制台的多个子窗口，能够一边看源码一边调试

-S -s 使得qemu在执行第一条指令之前停下来，然后sleep两秒给qemu充分的时间准备等待连接，接下来使用gdb调试工具，-tui提供代码与命令行分屏查看的界面，tools/gdbinit中存放的事gdb调试
```

一开始进入`tools\gdbinit`文件夹下修改`gdbinit`

```bash
set arch i8086
target post: 1234
```

## 练习3:分析bootloader进入保护模式的过程



