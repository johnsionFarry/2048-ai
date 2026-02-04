#ifndef GAME2048_H
#define GAME2048_H

#include <stdint.h>
#include <stdbool.h>

class Game2048 {
private:
    uint8_t height;
    uint8_t width;
    uint8_t **board;
    uint32_t score;
    uint8_t scheme;
    bool running;
    
    // 初始化棋盘内存
    void initMemory();
    
    // 释放棋盘内存
    void freeMemory();
    
    // 根据方块值和颜色方案获取前景色和背景色
    void getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background);
    
    // 计算数字的位数
    uint8_t getDigitCount(uint32_t number);
    
    // 查找数组中指定位置元素的目标位置（用于合并或移动）
    uint8_t findTarget(uint8_t *array, uint8_t x, uint8_t stop);
    
    // 滑动数组元素（用于实现游戏中的移动操作）
    bool slideArray(uint8_t *array, uint32_t *score);
    
    // 旋转棋盘（用于实现不同方向的移动）
    void rotateBoard();
    
    // 检查垂直方向是否有可合并的方块对
    bool findPairDown();
    
    // 计算棋盘上空方块的数量
    uint8_t countEmpty();
    
    // 设置终端输入模式
    void setBufferedInput(bool enable);
    
    // 信号处理函数
    static void signal_callback_handler(int signum);
    
public:
    // 构造函数
    Game2048(uint8_t height = 4, uint8_t width = 4, uint8_t scheme = 0);
    
    // 析构函数
    ~Game2048();
    
    // 初始化游戏
    void init();
    
    // 运行游戏
    void run();
    
    // 停止游戏
    void stop();
    
    // 向上移动方块
    bool moveUp();
    
    // 向左移动方块
    bool moveLeft();
    
    // 向下移动方块
    bool moveDown();
    
    // 向右移动方块
    bool moveRight();
    
    // 在棋盘上随机添加一个新方块（2或4）
    void addRandom();
    
    // 检查游戏是否结束
    bool gameEnded();
    
    // 绘制游戏棋盘
    void drawBoard();
    
    // 重置游戏
    void reset();
    
    // 获取棋盘高度
    uint8_t getHeight() const;
    
    // 获取棋盘宽度
    uint8_t getWidth() const;
    
    // 获取当前得分
    uint32_t getScore() const;
    
    // 获取棋盘内容（返回指定位置的值）
    uint8_t getCellValue(uint8_t row, uint8_t col) const;
    
    // 获取整个棋盘内容（返回二维数组的副本）
    uint8_t** getBoard() const;
    
    // 测试滑动数组功能是否正常工作
    bool testSucceed();
};

#endif // GAME2048_H