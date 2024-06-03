//指令0x5xy0不一样
//待会查一下0x8xye
//0x9xy0不一样

#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include "SDL.h"

#define u8 uint8_t  //1B
#define u16 uint16_t    //2B
#define u32 uint32_t    //4B
#define u64 uint64_t    //8B


//状态枚举
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

//指令类型, 单个指令固定2B
typedef struct {
    u16 opcode; //指令
    u16 NNN;    //指令低12位
    u8 NN;   //指令低8位
    u8 N;   //指令低4位
    u8 X;   //指令中高字节的低4位
    u8 Y;   //指令中低字节的高4位
} instruction_t;

//chip8类型
typedef struct {
    u8 V[16]; //V0~VF
    u16 stk[16];    //栈, 栈中存的是指令的地址, 即PC
    u16 *SP;  //指向栈顶, 相关文档中写的是8位的寄存器, 不过在模拟中为了方便把stk的地址赋给它就定义为16位的
    u16 I;  //索引寄存器, 只用了12位
    u16 PC; //程序计数器, 只用了12位, 存的是指令的地址
    u8 delay_timer;    //延迟计时器
    u8 sound_timer;    //声音计时器
    u8 ram[4096];   //内存0x000~0xFFF
    bool display[64 * 32];   //屏幕, 共2048b, 表示每个像素是否会被渲染
    u32 pixel_color[64 * 32];   //存储每个像素的颜色信息
    bool keypad[16];   //键盘, 0~F
    const char *rom_name;   //当前运行的游戏
    instruction_t inst; //当前正在执行的指令
    bool draw;  //是否渲染窗口
    emulator_state_t state; //虚拟机当前状态
} chip8_t;

//sdl的一些设置
typedef struct {
    SDL_Window *window; //窗口
    SDL_Renderer *renderer; //渲染器
    
    /*
    在调用 SDL_OpenAudioDevice 之后，have 结构将包含实际的音频设备配置。
    如果音频设备不完全支持 want 中的配置，SDL 会尽量匹配，并在 have 中返回实际使用的配置。
    */
    SDL_AudioSpec want, have;   //音频规范

    SDL_AudioDeviceID dev;  //音频设备ID
} sdl_t;

//配置
typedef struct {
    u32 window_width;  //窗口宽度
    u32 window_height; //窗口高度
    u32 fg_color;  //前景色
    u32 bg_color;  //背景色
    u32 scale_factor;  //缩放比例
    bool pixel_outlines;    //是否绘制像素边框
    u32 insts_per_second;  //chip8 CPU的时钟频率, 即每秒执行的指令数
    u32 square_wave_freq;  //方波声音频率，例如 440Hz 表示中音 A
    u32 audio_sample_rate; //音频采样率, 每秒钟对连续信号进行采样的次数, 以赫兹Hz为单位
    u16 volume; //音量大小
} config_t;

bool init_sdl(sdl_t *sdl, config_t *config);                          // 初始化SDL库
bool set_config_from_args(config_t *config, const int argc, char **argv);   //设置配置
void final_cleanup();   //最后退出
void clear_screen(const sdl_t sdl, const config_t config);   //清除屏幕 / 屏幕变为背景色
void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8); //更新屏幕
void handle_input(chip8_t *chip8, config_t *config);    //处理输入
bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]);
void emulate_instruction(chip8_t *chip8, const config_t config);    //模拟单条指令执行
//音频回调函数用于实时处理和生成音频数据。它是一个用户定义的函数，当音频设备需要新的音频数据时，音频系统会调用这个函数, 他向音频缓冲区填充数据
//userdata 指向用户数据的指针; stream 指向音频缓冲区的指针; len 缓冲区的长度, 以字节为单位
void audio_callback(void *userdata, uint8_t *stream, int len);  /* **音频在计算机内的生成** */

#endif //CHIP8_H