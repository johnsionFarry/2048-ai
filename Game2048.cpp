#include "Game2048.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <signal.h>

// 全局游戏实例指针，用于信号处理
static Game2048 *gameInstance = nullptr;

// 构造函数
Game2048::Game2048(uint8_t height, uint8_t width, uint8_t scheme) {
    this->height = height;
    this->width = width;
    this->scheme = scheme;
    this->score = 0;
    this->running = false;
    this->board = nullptr;
    
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 保存全局实例指针
    gameInstance = this;
    
    // 初始化内存
    initMemory();
}

// 析构函数
Game2048::~Game2048() {
    freeMemory();
}

// 初始化棋盘内存
void Game2048::initMemory() {
    board = (uint8_t **)malloc(height * sizeof(uint8_t *));
    for (uint8_t i = 0; i < height; i++) {
        board[i] = (uint8_t *)malloc(width * sizeof(uint8_t));
    }
}

// 释放棋盘内存
void Game2048::freeMemory() {
    if (board) {
        for (uint8_t i = 0; i < height; i++) {
            free(board[i]);
        }
        free(board);
        board = nullptr;
    }
}

// 根据方块值和颜色方案获取前景色和背景色
void Game2048::getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background) {
    uint8_t original[] = {8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0};
    uint8_t blackwhite[] = {232, 255, 234, 255, 236, 255, 238, 255, 240, 255, 242, 255, 244, 255, 246, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255, 0};
    uint8_t bluered[] = {235, 255, 63, 255, 57, 255, 93, 255, 129, 255, 165, 255, 201, 255, 200, 255, 199, 255, 198, 255, 197, 255, 196, 255, 196, 255, 196, 255, 196, 255, 196, 255};
    uint8_t *schemes[] = {original, blackwhite, bluered};
    
    *foreground = *(schemes[scheme] + (1 + value * 2) % sizeof(original));
    *background = *(schemes[scheme] + (0 + value * 2) % sizeof(original));
}

// 计算数字的位数
uint8_t Game2048::getDigitCount(uint32_t number) {
    uint8_t count = 0;
    do {
        number /= 10;
        count += 1;
    } while (number);
    return count;
}

// 查找数组中指定位置元素的目标位置（用于合并或移动）
uint8_t Game2048::findTarget(uint8_t *array, uint8_t x, uint8_t stop) {
    uint8_t t;
    if (x == 0) {
        return x;
    }
    
    for (t = x - 1;; t--) {
        if (array[t] != 0) {
            if (array[t] != array[x]) {
                return t + 1;
            }
            return t;
        } else {
            if (t == stop) {
                return t;
            }
        }
    }
    return x;
}

// 滑动数组元素（用于实现游戏中的移动操作）
bool Game2048::slideArray(uint8_t *array, uint32_t *score) {
    bool success = false;
    uint8_t x, t, stop = 0;

    for (x = 0; x < width; x++) {
        if (array[x] != 0) {
            t = findTarget(array, x, stop);
            if (t != x) {
                if (array[t] == 0) {
                    array[t] = array[x];
                } else if (array[t] == array[x]) {
                    array[t]++;
                    *score += 1 << array[t];
                    stop = t + 1;
                }
                array[x] = 0;
                success = true;
            }
        }
    }
    return success;
}

// 旋转棋盘（用于实现不同方向的移动）
void Game2048::rotateBoard() {
    uint8_t i, j;
    uint8_t **rotated = (uint8_t **)malloc(height * sizeof(uint8_t *));
    for (i = 0; i < height; i++) {
        rotated[i] = (uint8_t *)malloc(width * sizeof(uint8_t));
    }
    
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            rotated[j][height - i - 1] = board[i][j];
        }
    }
    
    for (i = 0; i < height; i++) {
        memcpy(board[i], rotated[i], width * sizeof(uint8_t));
        free(rotated[i]);
    }
    free(rotated);
    
    uint8_t temp = height;
    height = width;
    width = temp;
}

// 检查垂直方向是否有可合并的方块对
bool Game2048::findPairDown() {
    for (uint8_t x = 0; x < width; x++) {
        for (uint8_t y = 0; y < height - 1; y++) {
            if (board[y][x] == board[y + 1][x]) {
                return true;
            }
        }
    }
    return false;
}

// 计算棋盘上空方块的数量
uint8_t Game2048::countEmpty() {
    uint8_t count = 0;
    for (uint8_t y = 0; y < height; y++) {
        for (uint8_t x = 0; x < width; x++) {
            if (board[y][x] == 0) {
                count++;
            }
        }
    }
    return count;
}

// 设置终端输入模式
void Game2048::setBufferedInput(bool enable) {
    static bool enabled = true;
    static struct termios old;
    struct termios newt;

    if (enable && !enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        enabled = true;
    } else if (!enable && enabled) {
        tcgetattr(STDIN_FILENO, &newt);
        old = newt;
        newt.c_lflag &= (~ICANON & ~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        enabled = false;
    }
}

// 信号处理函数
void Game2048::signal_callback_handler(int signum) {
    if (gameInstance) {
        printf("         TERMINATED         \n");
        gameInstance->setBufferedInput(true);
        printf("\033[?25h\033[m");
        gameInstance->stop();
    }
    exit(signum);
}

// 初始化游戏
void Game2048::init() {
    // 清空棋盘
    for (uint8_t y = 0; y < height; y++) {
        for (uint8_t x = 0; x < width; x++) {
            board[y][x] = 0;
        }
    }
    
    // 添加两个随机方块
    addRandom();
    addRandom();
    
    // 重置得分
    score = 0;
}

// 运行游戏
void Game2048::run() {
    running = true;
    
    // 使光标不可见，清除整个屏幕
    printf("\033[?25l\033[2J");

    // 注册信号处理程序
    signal(SIGINT, signal_callback_handler);

    // 初始化游戏
    init();
    
    // 设置非缓冲输入
    setBufferedInput(false);
    
    // 绘制初始棋盘
    drawBoard();
    
    while (running) {
        int c = getchar();
        if (c == EOF) {
            puts("\nError! Cannot read keyboard input!");
            break;
        }
        
        bool success = false;
        
        // 处理方向键和其他控制键
        switch (c) {
        case 52:  // '4' key
        case 97:  // 'a' key
        case 104: // 'h' key
        case 68:  // left arrow
            success = moveLeft();
            break;
        case 54:  // '6' key
        case 100: // 'd' key
        case 108: // 'l' key
        case 67:  // right arrow
            success = moveRight();
            break;
        case 56:  // '8' key
        case 119: // 'w' key
        case 107: // 'k' key
        case 65:  // up arrow
            success = moveUp();
            break;
        case 50:  // '2' key
        case 115: // 's' key
        case 106: // 'j' key
        case 66:  // down arrow
            success = moveDown();
            break;
        default:
            success = false;
        }
        
        if (success) {
            drawBoard();
            usleep(150 * 1000);
            addRandom();
            drawBoard();
            if (gameEnded()) {
                printf("         GAME OVER          \n");
                break;
            }
        }
        
        // 处理退出命令
        if (c == 'q') {
            printf("        QUIT? (y/n)         \n");
            c = getchar();
            if (c == 'y') {
                break;
            }
            drawBoard();
        }
        
        // 处理重新开始命令
        if (c == 'r') {
            printf("       RESTART? (y/n)       \n");
            c = getchar();
            if (c == 'y') {
                init();
            }
            drawBoard();
        }
    }
    
    setBufferedInput(true);
    printf("\033[?25h\033[m");
    running = false;
}

// 停止游戏
void Game2048::stop() {
    running = false;
}

// 向上移动方块
bool Game2048::moveUp() {
    bool success = false;
    uint8_t x;
    for (x = 0; x < width; x++) {
        uint8_t *column = (uint8_t *)malloc(height * sizeof(uint8_t));
        for (uint8_t y = 0; y < height; y++) {
            column[y] = board[y][x];
        }
        
        success |= slideArray(column, &score);
        
        for (uint8_t y = 0; y < height; y++) {
            board[y][x] = column[y];
        }
        free(column);
    }
    return success;
}

// 向左移动方块
bool Game2048::moveLeft() {
    bool success = false;
    for (uint8_t y = 0; y < height; y++) {
        success |= slideArray(board[y], &score);
    }
    return success;
}

// 向下移动方块
bool Game2048::moveDown() {
    bool success = false;
    uint8_t x;
    for (x = 0; x < width; x++) {
        uint8_t *column = (uint8_t *)malloc(height * sizeof(uint8_t));
        for (uint8_t y = 0; y < height; y++) {
            column[height - y - 1] = board[y][x];
        }
        
        success |= slideArray(column, &score);
        
        for (uint8_t y = 0; y < height; y++) {
            board[y][x] = column[height - y - 1];
        }
        free(column);
    }
    return success;
}

// 向右移动方块
bool Game2048::moveRight() {
    bool success = false;
    for (uint8_t y = 0; y < height; y++) {
        uint8_t *row = (uint8_t *)malloc(width * sizeof(uint8_t));
        for (uint8_t x = 0; x < width; x++) {
            row[width - x - 1] = board[y][x];
        }
        
        success |= slideArray(row, &score);
        
        for (uint8_t x = 0; x < width; x++) {
            board[y][x] = row[width - x - 1];
        }
        free(row);
    }
    return success;
}

// 在棋盘上随机添加一个新方块（2或4）
void Game2048::addRandom() {
    uint8_t x, y;
    uint8_t r, len = 0;
    uint8_t n;
    
    len = countEmpty();
    if (len == 0) {
        return;
    }
    
    uint8_t **list = (uint8_t **)malloc(len * sizeof(uint8_t *));
    for (uint8_t i = 0; i < len; i++) {
        list[i] = (uint8_t *)malloc(2 * sizeof(uint8_t));
    }
    
    len = 0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (board[y][x] == 0) {
                list[len][0] = y;
                list[len][1] = x;
                len++;
            }
        }
    }

    if (len > 0) {
        r = rand() % len;
        y = list[r][0];
        x = list[r][1];
        n = (rand() % 10) / 9 + 1;
        board[y][x] = n;
    }
    
    for (uint8_t i = 0; i < len; i++) {
        free(list[i]);
    }
    free(list);
}

// 检查游戏是否结束
bool Game2048::gameEnded() {
    if (countEmpty() > 0) {
        return false;
    }
    
    if (findPairDown()) {
        return false;
    }
    
    for (uint8_t y = 0; y < height; y++) {
        for (uint8_t x = 0; x < width - 1; x++) {
            if (board[y][x] == board[y][x + 1]) {
                return false;
            }
        }
    }
    
    return true;
}

// 绘制游戏棋盘
void Game2048::drawBoard() {
    uint8_t x, y, fg, bg;
    printf("\033[H");
    printf("2048.c %17u pts\n\n", score);
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            getColors(board[y][x], scheme, &fg, &bg);
            printf("\033[38;5;%u;48;5;%um", fg, bg);
            printf("       ");
            printf("\033[m");
        }
        printf("\n");
        
        for (x = 0; x < width; x++) {
            getColors(board[y][x], scheme, &fg, &bg);
            printf("\033[38;5;%u;48;5;%um", fg, bg);
            if (board[y][x] != 0) {
                uint32_t number = 1 << board[y][x];
                uint8_t t = 7 - getDigitCount(number);
                printf("%*s%u%*s", t - t / 2, "", number, t / 2, "");
            } else {
                printf("   ·   ");
            }
            printf("\033[m");
        }
        printf("\n");
        
        for (x = 0; x < width; x++) {
            getColors(board[y][x], scheme, &fg, &bg);
            printf("\033[38;5;%u;48;5;%um", fg, bg);
            printf("       ");
            printf("\033[m");
        }
        printf("\n");
    }
    printf("\n");
    printf("        ←,↑,→,↓ or q        \n");
    printf("\033[A");
}

// 重置游戏
void Game2048::reset() {
    init();
}

// 获取棋盘高度
uint8_t Game2048::getHeight() const {
    return height;
}

// 获取棋盘宽度
uint8_t Game2048::getWidth() const {
    return width;
}

// 获取当前得分
uint32_t Game2048::getScore() const {
    return score;
}

// 获取棋盘内容（返回指定位置的值）
uint8_t Game2048::getCellValue(uint8_t row, uint8_t col) const {
    if (row < height && col < width) {
        return board[row][col];
    }
    return 0;
}

// 获取整个棋盘内容（返回二维数组的副本）
uint8_t** Game2048::getBoard() const {
    uint8_t **copy = (uint8_t **)malloc(height * sizeof(uint8_t *));
    for (uint8_t i = 0; i < height; i++) {
        copy[i] = (uint8_t *)malloc(width * sizeof(uint8_t));
        memcpy(copy[i], board[i], width * sizeof(uint8_t));
    }
    return copy;
}

// 测试滑动数组功能是否正常工作
bool Game2048::testSucceed() {
    if (height != 4 || width != 4) {
        printf("Tests only work with 4x4 board\n");
        return false;
    }
    
    uint8_t array[4];
    uint8_t data[] = {
        0, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 1, 1, 2, 0, 0, 0, 4,
        0, 1, 0, 1, 2, 0, 0, 0, 4,
        1, 0, 0, 1, 2, 0, 0, 0, 4,
        1, 0, 1, 0, 2, 0, 0, 0, 4,
        1, 1, 1, 0, 2, 1, 0, 0, 4,
        1, 0, 1, 1, 2, 1, 0, 0, 4,
        1, 1, 0, 1, 2, 1, 0, 0, 4,
        1, 1, 1, 1, 2, 2, 0, 0, 8,
        2, 2, 1, 1, 3, 2, 0, 0, 12,
        1, 1, 2, 2, 2, 3, 0, 0, 12,
        3, 0, 1, 1, 3, 2, 0, 0, 4,
        2, 0, 1, 1, 2, 2, 0, 0, 4};
    uint8_t *in, *out, *points;
    uint8_t t, tests;
    uint8_t i;
    bool success = true;
    uint32_t testScore;

    tests = (sizeof(data) / sizeof(data[0])) / (2 * 4 + 1);
    for (t = 0; t < tests; t++) {
        in = data + t * (2 * 4 + 1);
        out = in + 4;
        points = in + 2 * 4;
        for (i = 0; i < 4; i++) {
            array[i] = in[i];
        }
        testScore = 0;
        success &= slideArray(array, &testScore);
        for (i = 0; i < 4; i++) {
            if (array[i] != out[i]) {
                success = false;
            }
        }
        if (testScore != *points) {
            success = false;
        }
        if (success == false) {
            for (i = 0; i < 4; i++) {
                printf("%u ", in[i]);
            }
            printf("=> ");
            for (i = 0; i < 4; i++) {
                printf("%u ", array[i]);
            }
            printf("(%u points) expected ", testScore);
            for (i = 0; i < 4; i++) {
                printf("%u ", in[i]);
            }
            printf("=> ");
            for (i = 0; i < 4; i++) {
                printf("%u ", out[i]);
            }
            printf("(%u points)\n", *points);
            break;
        }
    }
    if (success) {
        printf("All %u tests executed successfully\n", tests);
    }
    return success;
}