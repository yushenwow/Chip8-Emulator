#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chip8.h"

#define DEBUG

void audio_callback(void *userdata, u8 *stream, int len) {
    config_t *config = (config_t *)userdata;
    
    u16 *audio_data = (u16 *)stream;    //16是因为每次填充两字节; 有符号是因为音频数据可以有振幅, 振幅可正可负
    static u32 running_sample_index = 0;   //用于跟踪生成音频的样本索引
    const u32 square_wave_period = config->audio_sample_rate / config->square_wave_freq;    //方波周期, 音频采样率除以方波频率
    const u32 half_square_wave_period = square_wave_period / 2; //方波的一般周期, 用于确定方波的高低电平

    //向SDL的音频缓冲区以每次 2 字节的频率填充数据, 所以len除以2
    //对于缓冲区的每个样本, 检查其在方波周期内(一个周期内, 方波有一半在高电平, 一般在低电平)的位置来输出音量, 处于方波的高电平就输出正音量, 反之输出负音量
    for (int i = 0; i < len / 2; i++)
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ? config->volume : -config->volume;
}

// 用传入的参数设置初始的模拟器配置
bool set_config_from_args(config_t *config, const int argc, char **argv)
{
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF, // 白色
        .bg_color = 0x0000000F, // 黑色
        .scale_factor = 20,
        .pixel_outlines = true,     // 绘制像素边框
        .insts_per_second = 600,    // 每秒执行600条指令
        .square_wave_freq = 440,    // 方波频率: 440Hz
        .audio_sample_rate = 44100, // 采样率: 44100Hz
        .volume = 3000,             // INI16_MAX是最大音量
    };

    for (int i = 1; i < argc; i++)
    {
        (void)argv[i]; // 这也是为了防止编译器报错

        /*
        这一行代码的作用是检查命令行参数 argv[i] 是否以 --scale-factor 开头。
        如果是的话，函数 strncmp 返回0，从而表明匹配成功。
        */
        // 以设置scale factor为例
        if (strncmp(argv[i], "--scale-factor", strlen("--scale-factor")) == 0)
        {
            i++;
            config->scale_factor = (u32)strtol(argv[i], NULL, 10); // strtol: 将字符串转为长整型
        }
    }

    return true; // 成功
}

bool init_sdl(sdl_t *sdl, config_t *config) {
    //1.初始化SDL库
    //等于0说明初始化成功
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL库初始化失败: %s\n", SDL_GetError());
        return false;
    }

    //2.初始化窗口
    
    sdl->window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   config->window_width * config->scale_factor,
                                   config->window_height * config->scale_factor,
                                   0);

    if (!sdl->window)
    {
        SDL_Log("无法创建SDL窗口 %s\n", SDL_GetError());
        return false;
    }

    //3.初始化渲染器
    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl->renderer) {
        SDL_Log("无法创建SDL渲染器 %s\n", SDL_GetError());
        return false;
    }

    // 4.初始化音频相关
    sdl->want = (SDL_AudioSpec) {
        .freq = 44100,  //44100Hz
        .format = AUDIO_S16LSB, //有符号16位小端
        .channels = 1,  //Mono(单声道, stereo(2)是立体声, 双声道)
        .samples = 512, //缓冲区大小
        .callback = audio_callback,
        .userdata = config, //将用户数据传递到音频回调函数
    };

    sdl->dev = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);

    if (sdl->dev == 0) {
        SDL_Log("无法打开音频设备 %s\n", SDL_GetError());
        return false;
    }

    if ((sdl->want.format != sdl->have.format) || (sdl->want.channels != sdl->have.channels)) {
        SDL_Log("无法获取期望的音频设置\n");
        return false;
    }

    return true;    //成功初始化
}

void final_cleanup(const sdl_t sdl) {
    SDL_DestroyRenderer(sdl.renderer);     //关闭渲染器
    SDL_DestroyWindow(sdl.window);    //关闭窗口
    SDL_CloseAudioDevice(sdl.dev);  //关闭音频设备
    SDL_Quit();
}

void clear_screen(const sdl_t sdl, const config_t config) {
    //将bg_color(32位)分成四段即背景色的RGB组成, a为透明度
    const u8 r = (config.bg_color >> 24) & 0xFF;   //相当于只留高8位, 下同理
    const u8 g = (config.bg_color >> 16) & 0xFF;
    const u8 b = (config.bg_color >>  8) & 0xFF;
    const u8 a = (config.bg_color >>  0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8) {
    //一个矩形
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    //获取背景色的值来绘制边框
    const u8 bg_r = (config.bg_color >> 24) & 0xFF;
    const u8 bg_g = (config.bg_color >> 16) & 0xFF;
    const u8 bg_b = (config.bg_color >>  8) & 0xFF;
    const u8 bg_a = (config.bg_color >>  0) & 0xFF;

    //每次遍历1个像素
    for (u32 i = 0; i < sizeof chip8->display; i++) {
        //把1维坐标i转化为二维左边(x, y)
        // x = i % width
        // y = i / width
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8->display[i]) {
            //用前景色绘制
            if (chip8->pixel_color[i] != config.fg_color)
                chip8->pixel_color[i] = config.fg_color;

            const u8 r = (chip8->pixel_color[i] >> 24) & 0xFF;
            const u8 g = (chip8->pixel_color[i] >> 16) & 0xFF;
            const u8 b = (chip8->pixel_color[i] >>  8) & 0xFF;
            const u8 a = (chip8->pixel_color[i] >>  0) & 0xFF;

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);    //绘制实心矩形

            if (config.pixel_outlines) {
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);    //绘制空心矩形, 即边框
            }
        }
        else {
            //display[i] == false, 用背景色绘制
            if (chip8->pixel_color[i] != config.bg_color)
                chip8->pixel_color[i] = config.bg_color;

            const u8 r = (chip8->pixel_color[i] >> 24) & 0xFF;
            const u8 g = (chip8->pixel_color[i] >> 16) & 0xFF;
            const u8 b = (chip8->pixel_color[i] >>  8) & 0xFF;
            const u8 a = (chip8->pixel_color[i] >>  0) & 0xFF;

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

/*
1 2 3 4      1 2 3 C 
q w e r  ->  4 5 6 D
a s d f      7 8 9 E
z x c v      A 0 B F
*/
void handle_input(chip8_t *chip8, config_t *config) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                chip8->state = QUIT;
                return;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: chip8->state = QUIT; break;
                    case SDLK_SPACE: //暂停
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                            puts("==== PAUSED ====");
                        }
                        else chip8->state = RUNNING;
                        break;
                    case SDLK_EQUALS:   //为当前游戏重置chip8虚拟机
                        init_chip8(chip8, *config, chip8->rom_name);
                        break;
                    case SDLK_DOWN: //降低音量
                        if (config->volume < INT16_MAX) config->volume -= 500;
                        break;
                    case SDLK_UP: //提高音量
                        if (config->volume < INT16_MAX) config->volume += 500;
                        break;
                    
                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;

                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;

                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;

                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;

                    default: break;
                }
                break;
            
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;

                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;

                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;

                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;

                    default: break;
                }
                break;

            default: break;
        }
    }
}

bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]) {
    const u16 entry = 0x200;  //chip8载入位置

    //字体数据
    const u8 font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80, // F
    };

    //1.初始化整个chip8虚拟机
    memset(chip8, 0, sizeof(chip8_t));

    //2.载入字体
    memcpy(&chip8->ram[0], font, sizeof(font));

    //读取并载入游戏
    //i 打开文件:
    FILE *rom = fopen(rom_name, "rb");  //"rb"以二进制模式读文件
    if (!rom) {
        SDL_Log("游戏: %s 打开失败\n", rom_name);
        return false;
    }
    
    //ii 读取游戏大小
    fseek(rom, 0, SEEK_END);    //将文件指针移动到文件末尾
    const size_t rom_size = ftell(rom); //获取文件指针当前位置
    const size_t max_size = sizeof(chip8->ram) - entry; //计算能加载的游戏大小上限
    rewind(rom);    //将文件指针重新移动到文件开头

    if (rom_size > max_size) {
        SDL_Log("这个游戏: %s 太大了, 游戏大小: %llu, 可加载上限: %llu\n", rom_name, rom_size, max_size);
        return false;
    }
    /* ftell():
        若流以二进制模式打开，则由此函数获得的值是从文件开始的字节数。
        若流以文本模式打开，则由此函数返回的值未指定，且仅若作为 fseek() 的输入才有意义。
    */

    //iii 加载游戏
    if (fread(&chip8->ram[entry], rom_size, 1, rom) != 1) {
        SDL_Log("无法将游戏: %s 读取到内存中\n", rom_name);
        return false;
    }
    fclose(rom);
    /* 
        size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
        从stream中读取nmemb个数据块并存储到ptr指向的位置, 每个数据块有size个字节
        成功则返回读取的数据块数量(nmemb); 如果返回值小于nmemb可能失败或以达到文件末尾
    */

    //设置chip8虚拟机
    chip8->state = RUNNING; //状态
    chip8->PC = entry;
    chip8->rom_name = rom_name;
    chip8->SP = &chip8->stk[0];
    memset(&chip8->pixel_color[0], config.bg_color, sizeof chip8->pixel_color);

    return true;
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8)
{
    printf("Address: 0x%04X, Opcode: 0x%04X Desc: ",
           chip8->PC - 2, chip8->inst.opcode);

    switch ((chip8->inst.opcode >> 12) & 0x0F)
    {
    case 0x00:
        if (chip8->inst.NN == 0xE0)
        {
            // 0x00E0: Clear the screen
            printf("Clear screen\n");
        }
        else if (chip8->inst.NN == 0xEE)
        {
            // 0x00EE: Return from subroutine
            // Set program counter to last address on subroutine stack ("pop" it off the stack)
            //   so that next opcode will be gotten from that address.
            printf("Return from subroutine to address 0x%04X\n",
                   *(chip8->SP - 1));
        }
        else
        {
            printf("Unimplemented Opcode.\n");
        }
        break;

    case 0x01:
        // 0x1NNN: Jump to address NNN
        printf("Jump to address NNN (0x%04X)\n",
               chip8->inst.NNN);
        break;

    case 0x02:
        // 0x2NNN: Call subroutine at NNN
        // Store current address to return to on subroutine stack ("push" it on the stack)
        //   and set program counter to subroutine address so that the next opcode
        //   is gotten from there.
        printf("Call subroutine at NNN (0x%04X)\n",
               chip8->inst.NNN);
        break;

    case 0x03:
        // 0x3XNN: Check if VX == NN, if so, skip the next instruction
        printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n",
               chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
        break;

    case 0x04:
        // 0x4XNN: Check if VX != NN, if so, skip the next instruction
        printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n",
               chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
        break;

    case 0x05:
        // 0x5XY0: Check if VX == VY, if so, skip the next instruction
        printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n",
               chip8->inst.X, chip8->V[chip8->inst.X],
               chip8->inst.Y, chip8->V[chip8->inst.Y]);
        break;

    case 0x06:
        // 0x6XNN: Set register VX to NN
        printf("Set register V%X = NN (0x%02X)\n",
               chip8->inst.X, chip8->inst.NN);
        break;

    case 0x07:
        // 0x7XNN: Set register VX += NN
        printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n",
               chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN,
               chip8->V[chip8->inst.X] + chip8->inst.NN);
        break;

    case 0x08:
        switch (chip8->inst.N)
        {
        case 0:
            // 0x8XY0: Set register VX = VY
            printf("Set register V%X = V%X (0x%02X)\n",
                   chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
            break;

        case 1:
            // 0x8XY1: Set register VX |= VY
            printf("Set register V%X (0x%02X) |= V%X (0x%02X); Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
            break;

        case 2:
            // 0x8XY2: Set register VX &= VY
            printf("Set register V%X (0x%02X) &= V%X (0x%02X); Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
            break;

        case 3:
            // 0x8XY3: Set register VX ^= VY
            printf("Set register V%X (0x%02X) ^= V%X (0x%02X); Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
            break;

        case 4:
            // 0x8XY4: Set register VX += VY, set VF to 1 if carry
            printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                   ((u16)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255));
            break;

        case 5:
            // 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive/0)
            printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                   (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
            break;

        case 6:
            // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF
            printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->V[chip8->inst.X] & 1,
                   chip8->V[chip8->inst.X] >> 1);
            break;

        case 7:
            // 0x8XY7: Set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive/0)
            printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                   chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
                   (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
            break;

        case 0xE:
            // 0x8XYE: Set register VX <<= 1, store shifted off bit in VF
            printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   (chip8->V[chip8->inst.X] & 0x80) >> 7,
                   chip8->V[chip8->inst.X] << 1);
            break;

        default:
            // Wrong/unimplemented opcode
            break;
        }
        break;

    case 0x09:
        // 0x9XY0: Check if VX != VY; Skip next instruction if so
        printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n",
               chip8->inst.X, chip8->V[chip8->inst.X],
               chip8->inst.Y, chip8->V[chip8->inst.Y]);
        break;

    case 0x0A:
        // 0xANNN: Set index register I to NNN
        printf("Set I to NNN (0x%04X)\n",
               chip8->inst.NNN);
        break;

    case 0x0B:
        // 0xBNNN: Jump to V0 + NNN
        printf("Set PC to V0 (0x%02X) + NNN (0x%04X); Result PC = 0x%04X\n",
               chip8->V[0], chip8->inst.NNN, chip8->V[0] + chip8->inst.NNN);
        break;

    case 0x0C:
        // 0xCXNN: Sets register VX = rand() % 256 & NN (bitwise AND)
        printf("Set V%X = rand() %% 256 & NN (0x%02X)\n",
               chip8->inst.X, chip8->inst.NN);
        break;

    case 0x0D:
        // 0xDXYN: Draw N-height sprite at coords X,Y; Read from memory location I;
        //   Screen pixels are XOR'd with sprite bits,
        //   VF (Carry flag) is set if any screen pixels are set off; This is useful
        //   for collision detection or other reasons.
        printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
               "from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n",
               chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y,
               chip8->V[chip8->inst.Y], chip8->I);
        break;

    case 0x0E:
        if (chip8->inst.NN == 0x9E)
        {
            // 0xEX9E: Skip next instruction if key in VX is pressed
            printf("Skip next instruction if key in V%X (0x%02X) is pressed; Keypad value: %d\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);
        }
        else if (chip8->inst.NN == 0xA1)
        {
            // 0xEX9E: Skip next instruction if key in VX is not pressed
            printf("Skip next instruction if key in V%X (0x%02X) is not pressed; Keypad value: %d\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);
        }
        break;

    case 0x0F:
        switch (chip8->inst.NN)
        {
        case 0x0A:
            // 0xFX0A: VX = get_key(); Await until a keypress, and store in VX
            printf("Await until a key is pressed; Store key in V%X\n",
                   chip8->inst.X);
            break;

        case 0x1E:
            // 0xFX1E: I += VX; Add VX to register I. For non-Amiga CHIP8, does not affect VF
            printf("I (0x%04X) += V%X (0x%02X); Result (I): 0x%04X\n",
                   chip8->I, chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->I + chip8->V[chip8->inst.X]);
            break;

        case 0x07:
            // 0xFX07: VX = delay timer
            printf("Set V%X = delay timer value (0x%02X)\n",
                   chip8->inst.X, chip8->delay_timer);
            break;

        case 0x15:
            // 0xFX15: delay timer = VX
            printf("Set delay timer value = V%X (0x%02X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X]);
            break;

        case 0x18:
            // 0xFX18: sound timer = VX
            printf("Set sound timer value = V%X (0x%02X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X]);
            break;

        case 0x29:
            // 0xFX29: Set register I to sprite location in memory for character in VX (0x0-0xF)
            printf("Set I to sprite location in memory for character in V%X (0x%02X). Result(VX*5) = (0x%02X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
            break;

        case 0x33:
            // 0xFX33: Store BCD representation of VX at memory offset from I;
            //   I = hundred's place, I+1 = ten's place, I+2 = one's place
            printf("Store BCD representation of V%X (0x%02X) at memory from I (0x%04X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
            break;

        case 0x55:
            // 0xFX55: Register dump V0-VX inclusive to memory offset from I;
            //   SCHIP does not inrement I, CHIP8 does increment I
            printf("Register dump V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
            break;

        case 0x65:
            // 0xFX65: Register load V0-VX inclusive from memory offset from I;
            //   SCHIP does not inrement I, CHIP8 does increment I
            printf("Register load V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
            break;

        default:
            break;
        }
        break;

    default:
        printf("Unimplemented Opcode.\n");
        break; // Unimplemented or invalid opcode
    }
}
#endif

void emulate_instruction(chip8_t *chip8, const config_t config) {
    bool carry; //VF的值, VF作为进位标志用于某些指令中

    //1.结合PC寄存器在内存中获取指令, 同时PC后移
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];  /* **这里涉及到类型转换, 移位运算, 大小端** */
    
    chip8->PC += 2;

    //2.将一条指令转为nnn,nn等
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; 
    chip8->inst.NN  = chip8->inst.opcode & 0x0FF;
    chip8->inst.N   = chip8->inst.opcode & 0x0F;
    chip8->inst.X   = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y   = (chip8->inst.opcode >> 4) & 0x0F;

    #ifdef DEBUG
        print_debug_info(chip8);
    #endif

    //3.模拟指令
    switch ((chip8->inst.opcode >> 12) & 0x0F)    //保留高四位
    {
    case 0x00:
        // 0x00E0: 清屏
        if (chip8->inst.NN == 0xE0) {
            memset(chip8->display, false, sizeof chip8->display);
            chip8->draw = true;
        }
        else if (chip8->inst.NN == 0xEE) {
            // 0x00EE: 从子程序返回   
            // 栈顶指针减一(相当于pop), 再将SP指向的内容(父程序调用子程序之后的地址)赋值给PC
            chip8->PC = *--chip8->SP;
        }
        else {
            // 0x0NNN: wiki上说大多数rom用不上
        }
        break;
    case 0x01: 
        // 0x1NNN: 跳转到NNN
        chip8->PC = chip8->inst.NNN;
        break;
    case 0x02:
        // 0x2NNN:调用位于NNN的子程序
        // 先将当前PC推入栈中, 再将当前PC设置为NNN
        *chip8->SP++ = chip8->PC;
        chip8->PC = chip8->inst.NNN;
        break;
    case 0x03:
        // 0x3XNN: 如果寄存器X的内容(VX) == NN, 跳过下一条指令
        if (chip8->V[chip8->inst.X] == chip8->inst.NN) chip8->PC += 2;
        break;
    case 0x04:
        // 0x4XNN: 如果VX != NN, 跳过下一条指令
        if (chip8->V[chip8->inst.X] != chip8->inst.NN) chip8->PC += 2;
        break;
    case 0x05:
        // 0x5XY0: 如果VX == VY, 跳过下一条指令
        if (chip8->inst.N == 0 && chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) 
            chip8->PC += 2;
        break;
    case 0x06:
        // 0x6XNN: 设置VX为NN
        chip8->V[chip8->inst.X] = chip8->inst.NN;
        break;
    case 0x07:
        // 0x7XNN: VX += NN, 进位标志不变
        chip8->V[chip8->inst.X] += chip8->inst.NN;
        break;
    case 0x08:
        switch (chip8->inst.N) {
            case 0x0:
                // 0x8XY0: VX = VY
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                break;
            case 0x1:
                // 0x8XY1: VX |= VY
                chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                chip8->V[0xF] = 0;  //chip888
                break;
            case 0x2:
                // 0x8XY2: VX &= VY
                chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                chip8->V[0xF] = 0; //chip888
                break;
            case 0x3:
                // 0x8XY1: VX ^= VY 异或
                chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                chip8->V[0xF] = 0; //chip888
                break;
            case 0x4:
                // 0x8XY1: VX += VY, 有溢出则置VF为1, 否则置0;
                carry = (u16)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 0xFF;
                chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                chip8->V[0x0F] = carry;
                break;
            case 0x5:
                // 0x8XY5: VX -= VY, 如果VX >= VY, 则VF = 1, 否则VF = 0
                carry = (chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]);
                chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                chip8->V[0x0F] = carry;
                break;
            case 0x6:
                // 0x8XY6: 将VY的最低有效位存储在VF中, 然后VX = VY >> 1
                carry = chip8->V[chip8->inst.Y] & 1;    //chip888
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;

                //carry = chip8->V[chip8->inst.X] & 1;
                //chip8->V[chip8->inst.X] >>= 1;

                chip8->V[0x0F] = carry;
                break;
            case 0x7:
                // 0x8XY7: 将VX的最低有效位存储在VF中, 然后VX >>= 1;
                carry = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                chip8->V[0x0F] = carry;
                break;
            case 0xE:
                // 0x8XYE: VF = (VY的最高有效位), 然后VX = VY << 1
                carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;  //chip888
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1;

                //carry = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                //chip8->V[chip8->inst.X] <<= 1;

                chip8->V[0x0F] = carry;
                break;

            default: break;
        }
        break;
    case 0x09:
        // 9XY0: 如果VX != VY, 则跳过下一条指令
        if (chip8->inst.N == 0 && chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
            chip8->PC += 2;
        break;
    case 0x0A:
        // 0xANNN: I = NNN
        chip8->I = chip8->inst.NNN;
        break;
    case 0x0B:
        // 0xBNNN: PC = V0 + NNN
        chip8->PC = chip8->V[0x0] + chip8->inst.NNN;
        break;
    case 0x0C:
        // 0xCXNN: VX = rand() & NN, 随机数范围:[0, 255]
        chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
        break;
    case 0x0D: {
        // 0xDXYN: 绘制一个字体, 从(x, y)开始绘制(XOR), 宽8位, 高N位, 即N行8列;
        //从内存I开始读取, I在执行指令之后不会改变;
        //如果发生碰撞, 置VF = 1, 否则置VF = 0
        //碰撞: 如果一个像素已经被渲染而它目前又要被渲染, 就发生了碰撞

        //1.起始位置和终点位置
        u8 X = chip8->V[chip8->inst.X] % config.window_width;
        u8 Y = chip8->V[chip8->inst.Y] % config.window_height;
        const u8 sX = X;

        chip8->V[0xF] = 0;

        //2.双层循环
        for (u8 i = 0; i < chip8->inst.N; i++) {
            const u8 sprite = chip8->ram[chip8->I + i];   //取1字节/ 1行8位
            X = sX; //重置X

            for (u8 j = 7; j >= 0; j--) {
                bool *pixel = &chip8->display[Y * config.window_width + X];  //取得当前屏幕上某个像素的指针, 代表该像素是否要被绘制
                const bool sprite_bit = sprite & (1 << j);  //取得读取到的图形中某个对应像素的值, 代表该像素是否要被绘制

                //发生碰撞
                if (sprite_bit && *pixel) chip8->V[0xF] = 1;

                *pixel ^= sprite_bit;
                
                //屏幕边缘
                if (++X >= config.window_width) break;
            }

            //屏幕边缘
            if (++Y >= config.window_height) break;
        }
        chip8->draw = true;

        break;
        }
    case 0x0E:
        if (chip8->inst.NN == 0x9E)  {
            // 0xEX9E: 如果VX中存储的键被按下, 跳过下一条指令
            if (chip8->keypad[chip8->V[chip8->inst.X]]) chip8->PC += 2;
        }
        else if (chip8->inst.NN == 0xA1) {
            // 0xEX9E: 如果VX中存储的键被按下, 跳过下一条指令
            if (chip8->keypad[chip8->V[chip8->inst.X]]) chip8->PC += 2;
        }

        break;
    case 0x0F:
        switch (chip8->inst.NN) {
        case 0x07:
            // 0xFX07: VX = delay_timer
            chip8->V[chip8->inst.X] = chip8->delay_timer;
            break;
        case 0x0A: {
            // 0xFX0A: 等待按键, 所有指令暂停, 直到按键, 将那个键存在VX
            static bool any = false;
            static u8 key = 0xFF;
            
            //遍历是否有键被按下
            for (u8 i = 0; key == 0xFF && i < sizeof chip8->keypad; i++) {
                if (chip8->keypad[i]) {
                    any = true;
                    key = i;
                    break;
                }
            }

            //遍历一遍后没有键被按下, 将PC往回调, 
            if (!any) chip8->PC -= 2;
            else {
                //有键被按下, 将之存在VX中
                if (chip8->keypad[key]) {
                    //如果这个键还没松开, 就继续回调PC
                    chip8->PC -= 2;
                }
                else {
                    chip8->V[chip8->inst.X] = key;
                    key = 0xFF;
                    any = false;
                }
            }
            break;
        }
        case 0x15:
            // 0xFX15: delay_timer = VX
            chip8->delay_timer = chip8->V[chip8->inst.X];
            break;
        case 0x18:
            // 0xFX18: sound_timer = VX;
            chip8->sound_timer = chip8->V[chip8->inst.X];
            break;
        case 0x1E:
            // 0xFX1E: I += VX, 不管VF
            chip8->I += chip8->V[chip8->inst.X];
            break;
        case 0x29:
            // 0xFX29: 将I设置为储存在VX中的值对应sprite字符的起始地址
            // 我们的字符位置: 0x000~0x04F, 一个sprite字符用5个字节存储, 共80字节
            chip8->I = chip8->V[chip8->inst.X] * 5;
            break;
        case 0x33:
            //0xFX33: 将VX的百位, 十位和个位以BCD码形式分别存在内存I, I + 1, I + 2中
            u8 bcd = chip8->V[chip8->inst.X];
            chip8->ram[chip8->I + 2] = bcd % 10;
            bcd /= 10;
            chip8->ram[chip8->I + 1] = bcd % 10;
            bcd /= 10;
            chip8->ram[chip8->I] = bcd;
            break;
        case 0x55:
            // 0xFX55: 从I开始存储V0~VX(包括VX), I会变化
            for (u8 i = 0; i <= chip8->inst.X; i++)
                chip8->ram[chip8->I++] = chip8->V[i];   //chip888
                //chip8->ram[chip8->I + i] = chip8->V[i];
            break;
        case 0x65:
            // 0xFX65: 从I开始, 往V0到VX中存, I会变化
            for (u8 i = 0; i <= chip8->inst.X; i++)
                chip8->V[i] = chip8->ram[chip8->I++];   //chip888
                //chip8->V[i] = chip8->ram[chip8->I + i];
            break;

        default: break;
        }
        break;

    default: break;
    }
}

//每60Hz更新一次timers
void update_timers(const sdl_t sdl, chip8_t *chip8) {
    if (chip8->delay_timer > 0) chip8->delay_timer--;

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl.dev, 0);   //播放
    }
    else SDL_PauseAudioDevice(sdl.dev, 1);  //暂停
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "使用: %s <rom_name> 的格式来运行\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    //1.初始化配置
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

    //2.初始化SDL库
    sdl_t sdl = {0};
    if (!init_sdl(&sdl, &config)) exit(EXIT_FAILURE);
    
    //3.初始化chip8虚拟机
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, config, rom_name)) exit(EXIT_FAILURE);

    //4.用背景色初始化屏幕
    clear_screen(sdl, config);

    //随机一个种子
    srand(time(NULL));

    //5.进入主循环
    while (chip8.state != QUIT) {
        //处理输入
        handle_input(&chip8, &config);

        if (chip8.state == PAUSED) continue;

        /* 1帧(Hz)的周期(frame, 时间)可以执行若干条指令 */

        //在运行指令之前获取时间, (frame表示周期, 代表1帧的执行时间)
        const uint64_t start_frame_time = SDL_GetPerformanceCounter();  //此函数用于计算时间差

        //模拟指令: "config.insts_per_second / 60"代表 60Hz, 1Hz执行config.insts_per_second / 60条指令
        //同理:"config.insts_per_second / 144"代表 144Hz, 1Hz执行config.insts_per_second / 144条指令
        for (u32 i = 0; i < config.insts_per_second / 60; i++) {
            emulate_instruction(&chip8, config);

            //指令中可能要求绘制sprite字体, 一个frame绘制1个sprite
            if (chip8.inst.opcode >> 12 == 0xD) break;  //chip888
        }

        //指令结束后获取时间
        const u64 end_frame_time = SDL_GetPerformanceCounter();

        //计算当前帧的实际执行时间, 确保每帧的执行时间接近16.67ms(即每秒60帧)
        const double time_elapsed = (double)((end_frame_time - start_frame_time) * 1000) / SDL_GetPerformanceFrequency();   //此函数返回计数器的频率, 即每秒钟的计数器刻度数, 也就是计数器每秒增加多少次

        //Delay: (大约)60hz/60fps(16.67ms), 根据当前帧的实际执行时间进行延迟, 如果不足16.67ms, 就延迟余下的时间, 否则不延迟
        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

        //渲染窗口
        if (chip8.draw) {
            update_screen(sdl, config, &chip8);
            chip8.draw = false;
        }
    }

    //6.最后退出  
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}

