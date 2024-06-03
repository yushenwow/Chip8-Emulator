# 实现一个Chip-8 Emulater

## 介绍

   起初是想跟着实现一个GBA模拟器, 但是由于过于复杂, 所以决定先从chip-8开始入手, 接下来是我的项目日志

**CHIP-8** 是一种[解释](https://en.wikipedia.org/wiki/Interpreter_(computing))[型编程语言](https://en.wikipedia.org/wiki/Programming_language)，由 [Joseph Weisbecker](https://en.wikipedia.org/wiki/Joseph_Weisbecker) 在他的 [1802](https://en.wikipedia.org/wiki/RCA_1802) 微处理器上开发。它最初在1970年代中期用于[COSMAC VIP](https://en.wikipedia.org/wiki/COSMAC_VIP)和[Telmac 1800](https://en.wikipedia.org/wiki/Telmac_1800) [8位](https://en.wikipedia.org/wiki/8-bit)[微型计算机](https://en.wikipedia.org/wiki/Microcomputer)。CHIP-8 [程序](https://en.wikipedia.org/wiki/Computer_program)在 CHIP-8 [虚拟机](https://en.wikipedia.org/wiki/Virtual_machine)上运行。它[旨在允许视频游戏](https://en.wikipedia.org/wiki/Video_game)更容易为这些计算机编程。([CHIP-8 - 维基百科，自由的百科全书 (wikipedia.org)](https://en.wikipedia.org/wiki/CHIP-8))

 

​	本质即一个虚拟机, 而任何一个虚拟机都需要: **指令集(instruction set)、寄存器(register)、内存(memory)、输入(input)、输出(output)** 



### 指令集(instruction set)

​	chip-8指令集中有**35条指令**, 每条指令固定占**2个字节** 

### 寄存器(register) 

1. chip-8共有**16个8-bit寄存器**, 记做 V0 ~ VF, 其中VF用于当做进位标志
2. 另有一个**索引寄存器(I)** 和一个**程序计数器(PC)** 
2. **两个8-bit timer**: **delay timer** 和 **sound timer**

### 内存(memory)

一共可以访问**4KB**内存, 地址从**0x000~0xfff** ,其中:

**0x000-0x1FF(前512B)** : 为解释器本身使用

**0x200-0xE9F** : 程序、数据等, 如所运行的游戏, 就从0x200开始

**0xEA0~0xEFF**: 用于调用栈和其他

**0xF00~0xFFF**: 用于显示刷新

### 输入(input)

![image-20240518152409232](C:\Users\玉罙\AppData\Roaming\Typora\typora-user-images\image-20240518152409232.png)

## 输出(显示): 

### 1.屏幕: **64*32bit** 

### 2.字体

​	十六进制数字显示, 0~F, 每个数字的图形用五个字节表示, 每个字节使用5位

![image-20240523192401604](C:\Users\玉罙\AppData\Roaming\Typora\typora-user-images\image-20240523192401604.png)

![image-20240523192415522](C:\Users\玉罙\AppData\Roaming\Typora\typora-user-images\image-20240523192415522.png)





## 环境

### 1.编辑器: VSCode in Win11

### 2.编译器: mingw-gcc

### 3.构建工具: Cmake

### 4.语言选择: C语言

### 5.音频、图形、输入: SDL2



## 参考

[用 C 实现一个 CHIP-8 模拟器 - CJ Ting's Blog ](https://cjting.me/2020/06/07/chip8-emulator/)

[如何创建自己的 Chip-8 仿真器 (freecodecamp.org)](https://www.freecodecamp.org/news/creating-your-very-own-chip-8-emulator/) 

[Cowgod's Chip-8 Technical Reference (free.fr)](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#2.2) 

