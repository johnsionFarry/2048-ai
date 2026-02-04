#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>
#include "Game2048.h"

// 全局游戏实例
Game2048 *game = nullptr;

// 游戏线程函数
void *gameThread(void *arg) {
    if (game) {
        game->run();
    }
    return nullptr;
}

// 显示帮助信息
void showHelp(const char *argv0) {
    printf("Usage: %s [OPTION] | [MODE]\n", argv0);
    printf("Play the game 2048 in the console\n\n");

    printf("Options:\n");
    printf("  -h, --help       Show this help message.\n");
    printf("  -v, --version    Show version number.\n");
    printf("  -H, --height     Set board height (default: 4).\n");
    printf("  -W, --width      Set board width (default: 4).\n\n");

    printf("Modes:\n");
    printf("  bluered      Use a blue-to-red color scheme (requires 256-color terminal support).\n");
    printf("  blackwhite   The black-to-white color scheme (requires 256-color terminal support).\n");
}

int main(int argc, char *argv[]) {
    uint8_t height = 4;
    uint8_t width = 4;
    uint8_t scheme = 0;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "hvH:W:")) != -1) {
        switch (opt) {
        case 'h':
            showHelp(argv[0]);
            return EXIT_SUCCESS;
        case 'v':
            printf("2048.c version 1.0.3\n");
            return EXIT_SUCCESS;
        case 'H':
            height = atoi(optarg);
            if (height < 2 || height > 20) {
                printf("Height must be between 2 and 20\n");
                return EXIT_FAILURE;
            }
            break;
        case 'W':
            width = atoi(optarg);
            if (width < 2 || width > 20) {
                printf("Width must be between 2 and 20\n");
                return EXIT_FAILURE;
            }
            break;
        default:
            printf("Invalid option\n");
            showHelp(argv[0]);
            return EXIT_FAILURE;
        }
    }
    
    // 处理剩余的位置参数
    if (optind < argc) {
        if (strcmp(argv[optind], "bluered") == 0) {
            scheme = 1;
        } else if (strcmp(argv[optind], "blackwhite") == 0) {
            scheme = 2;
        } else if (strcmp(argv[optind], "test") == 0) {
            // 测试模式
            Game2048 testGame(4, 4, 0);
            bool result = testGame.testSucceed();
            return result ? EXIT_SUCCESS : EXIT_FAILURE;
        } else {
            printf("Invalid mode: %s\n\nTry '%s --help' for more options.\n", argv[optind], argv[0]);
            return EXIT_FAILURE;
        }
    }
    
    // 创建游戏实例
    game = new Game2048(height, width, scheme);
    
    // 创建游戏线程
    pthread_t thread;
    int threadResult = pthread_create(&thread, nullptr, gameThread, nullptr);
    if (threadResult != 0) {
        printf("Error creating game thread\n");
        delete game;
        return EXIT_FAILURE;
    }
    
    // 等待游戏线程结束
    pthread_join(thread, nullptr);
    
    // 清理资源
    delete game;
    
    return EXIT_SUCCESS;
}