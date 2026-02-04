/*
 ============================================================================
 Name        : 2048.c
 Author      : Maurits van der Schee
 Description : Console version of the game "2048" for GNU/Linux
 ============================================================================
 */

#define VERSION "1.0.3"

#define _XOPEN_SOURCE 500 // for: usleep
#include <stdio.h>         // defines: printf, puts, getchar
#include <stdlib.h>         // defines: EXIT_SUCCESS, malloc, free
#include <string.h>         // defines: strcmp
#include <unistd.h>         // defines: STDIN_FILENO, usleep
#include <termios.h>        // defines: termios, TCSANOW, ICANON, ECHO
#include <stdbool.h>        // defines: true, false
#include <stdint.h>         // defines: uint8_t, uint32_t
#include <time.h>           // defines: time
#include <signal.h>         // defines: signal, SIGINT
#include <getopt.h>         // defines: getopt_long

// 全局变量，存储棋盘的高度和宽度
static uint8_t HEIGHT = 4;
static uint8_t WIDTH = 4;

/**
 * 根据方块值和颜色方案获取前景色和背景色
 * @param value 方块的值（指数形式，0表示空）
 * @param scheme 颜色方案索引
 * @param foreground 输出参数，前景色
 * @param background 输出参数，背景色
 */
void getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background)
{
	// 定义三种颜色方案：原始、黑白、蓝红
	uint8_t original[] = {8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0};
	uint8_t blackwhite[] = {232, 255, 234, 255, 236, 255, 238, 255, 240, 255, 242, 255, 244, 255, 246, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255, 0};
	uint8_t bluered[] = {235, 255, 63, 255, 57, 255, 93, 255, 129, 255, 165, 255, 201, 255, 200, 255, 199, 255, 198, 255, 197, 255, 196, 255, 196, 255, 196, 255, 196, 255, 196, 255};
	uint8_t *schemes[] = {original, blackwhite, bluered};
	
	// 根据值计算颜色索引并设置输出参数
	*foreground = *(schemes[scheme] + (1 + value * 2) % sizeof(original));
	*background = *(schemes[scheme] + (0 + value * 2) % sizeof(original));
}

/**
 * 计算数字的位数
 * @param number 要计算的数字
 * @return 数字的位数
 */
uint8_t getDigitCount(uint32_t number)
{
	uint8_t count = 0;
	do
	{
		number /= 10;
		count += 1;
	} while (number);
	return count;
}

/**
 * 绘制游戏棋盘
 * @param board 游戏棋盘数组
 * @param scheme 颜色方案索引
 * @param score 当前得分
 */
void drawBoard(uint8_t **board, uint8_t scheme, uint32_t score)
{
	uint8_t x, y, fg, bg;
	printf("\033[H"); // 移动光标到0,0
	printf("2048.c %17u pts\n\n", score); // 显示游戏标题和得分
	
	// 遍历棋盘的每一行
	for (y = 0; y < HEIGHT; y++)
	{
		// 绘制第一行（顶部边框）
		for (x = 0; x < WIDTH; x++)
		{
			getColors(board[y][x], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // 设置颜色
			printf("       ");
			printf("\033[m"); // 重置所有模式
		}
		printf("\n");
		
		// 绘制第二行（数字）
		for (x = 0; x < WIDTH; x++)
		{
			getColors(board[y][x], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // 设置颜色
			if (board[y][x] != 0)
			{
				uint32_t number = 1 << board[y][x]; // 将指数转换为实际值
				uint8_t t = 7 - getDigitCount(number);
				printf("%*s%u%*s", t - t / 2, "", number, t / 2, ""); // 居中显示数字
			}
			else
			{
				printf("   ·   "); // 空方块显示
			}
			printf("\033[m"); // 重置所有模式
		}
		printf("\n");
		
		// 绘制第三行（底部边框）
		for (x = 0; x < WIDTH; x++)
		{
			getColors(board[y][x], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // 设置颜色
			printf("       ");
			printf("\033[m"); // 重置所有模式
		}
		printf("\n");
	}
	printf("\n");
	printf("        ←,↑,→,↓ or q        \n"); // 显示操作提示
	printf("\033[A"); // 光标上移一行
}

/**
 * 查找数组中指定位置元素的目标位置（用于合并或移动）
 * @param array 要操作的数组
 * @param x 当前元素位置
 * @param stop 停止搜索的位置
 * @return 目标位置索引
 */
uint8_t findTarget(uint8_t *array, uint8_t x, uint8_t stop)
{
	uint8_t t;
	// 如果位置已经在第一个，不需要评估
	if (x == 0)
	{
		return x;
	}
	
	// 从当前位置向左搜索
	for (t = x - 1;; t--)
	{
		if (array[t] != 0)
		{
			if (array[t] != array[x])
			{
				// 无法合并，返回下一个位置
				return t + 1;
			}
			return t;
		}
		else
		{
			// 不能再滑动，返回当前位置
			if (t == stop)
			{
				return t;
			}
		}
	}
	// 未找到目标
	return x;
}

/**
 * 滑动数组元素（用于实现游戏中的移动操作）
 * @param array 要操作的数组
 * @param score 指向当前得分的指针，用于更新得分
 * @return 是否成功滑动
 */
bool slideArray(uint8_t *array, uint32_t *score)
{
	bool success = false;
	uint8_t x, t, stop = 0;

	for (x = 0; x < WIDTH; x++)
	{
		if (array[x] != 0)
		{
			t = findTarget(array, x, stop);
			// 如果目标不是原始位置，则移动或合并
			if (t != x)
			{
				// 如果目标为0，这是一个移动
				if (array[t] == 0)
				{
					array[t] = array[x];
				}
				else if (array[t] == array[x])
				{
					// 合并（增加2的幂次）
					array[t]++;
					// 增加得分
					*score += 1 << array[t];
					// 设置停止位置以避免双重合并
					stop = t + 1;
				}
				array[x] = 0;
				success = true;
			}
		}
	}
	return success;
}

/**
 * 旋转棋盘（用于实现不同方向的移动）
 * @param board 游戏棋盘数组
 */
void rotateBoard(uint8_t **board)
{
	uint8_t i, j;
	uint8_t **rotated = (uint8_t **)malloc(HEIGHT * sizeof(uint8_t *));
	for (i = 0; i < HEIGHT; i++)
	{
		rotated[i] = (uint8_t *)malloc(WIDTH * sizeof(uint8_t));
	}
	
	// 执行90度旋转
	for (i = 0; i < HEIGHT; i++)
	{
		for (j = 0; j < WIDTH; j++)
		{
			rotated[j][HEIGHT - i - 1] = board[i][j];
		}
	}
	
	// 复制旋转后的内容回原棋盘
	for (i = 0; i < HEIGHT; i++)
	{
		memcpy(board[i], rotated[i], WIDTH * sizeof(uint8_t));
		free(rotated[i]);
	}
	free(rotated);
	
	// 交换高度和宽度
	uint8_t temp = HEIGHT;
	HEIGHT = WIDTH;
	WIDTH = temp;
}

/**
 * 向上移动方块
 * @param board 游戏棋盘数组
 * @param score 指向当前得分的指针
 * @return 是否成功移动
 */
bool moveUp(uint8_t **board, uint32_t *score)
{
	bool success = false;
	uint8_t x;
	for (x = 0; x < WIDTH; x++)
	{
		// 创建临时数组存储当前列
		uint8_t *column = (uint8_t *)malloc(HEIGHT * sizeof(uint8_t));
		for (uint8_t y = 0; y < HEIGHT; y++)
		{
			column[y] = board[y][x];
		}
		
		// 滑动列
		success |= slideArray(column, score);
		
		// 复制回原棋盘
		for (uint8_t y = 0; y < HEIGHT; y++)
		{
			board[y][x] = column[y];
		}
		free(column);
	}
	return success;
}

/**
 * 向左移动方块
 * @param board 游戏棋盘数组
 * @param score 指向当前得分的指针
 * @return 是否成功移动
 */
bool moveLeft(uint8_t **board, uint32_t *score)
{
	bool success = false;
	for (uint8_t y = 0; y < HEIGHT; y++)
	{
		success |= slideArray(board[y], score);
	}
	return success;
}

/**
 * 向下移动方块
 * @param board 游戏棋盘数组
 * @param score 指向当前得分的指针
 * @return 是否成功移动
 */
bool moveDown(uint8_t **board, uint32_t *score)
{
	bool success = false;
	uint8_t x;
	for (x = 0; x < WIDTH; x++)
	{
		// 创建临时数组存储当前列
		uint8_t *column = (uint8_t *)malloc(HEIGHT * sizeof(uint8_t));
		for (uint8_t y = 0; y < HEIGHT; y++)
		{
			column[HEIGHT - y - 1] = board[y][x];
		}
		
		// 滑动列
		success |= slideArray(column, score);
		
		// 复制回原棋盘
		for (uint8_t y = 0; y < HEIGHT; y++)
		{
			board[y][x] = column[HEIGHT - y - 1];
		}
		free(column);
	}
	return success;
}

/**
 * 向右移动方块
 * @param board 游戏棋盘数组
 * @param score 指向当前得分的指针
 * @return 是否成功移动
 */
bool moveRight(uint8_t **board, uint32_t *score)
{
	bool success = false;
	for (uint8_t y = 0; y < HEIGHT; y++)
	{
		// 创建临时数组存储当前行的反转
		uint8_t *row = (uint8_t *)malloc(WIDTH * sizeof(uint8_t));
		for (uint8_t x = 0; x < WIDTH; x++)
		{
			row[WIDTH - x - 1] = board[y][x];
		}
		
		// 滑动行
		success |= slideArray(row, score);
		
		// 复制回原棋盘
		for (uint8_t x = 0; x < WIDTH; x++)
		{
			board[y][x] = row[WIDTH - x - 1];
		}
		free(row);
	}
	return success;
}

/**
 * 检查垂直方向是否有可合并的方块对
 * @param board 游戏棋盘数组
 * @return 是否存在可合并的方块对
 */
bool findPairDown(uint8_t **board)
{
	bool success = false;
	uint8_t x, y;
	for (x = 0; x < WIDTH; x++)
	{
		for (y = 0; y < HEIGHT - 1; y++)
		{
			if (board[y][x] == board[y + 1][x])
				return true;
		}
	}
	return success;
}

/**
 * 计算棋盘上空方块的数量
 * @param board 游戏棋盘数组
 * @return 空方块数量
 */
uint8_t countEmpty(uint8_t **board)
{
	uint8_t x, y;
	uint8_t count = 0;
	for (y = 0; y < HEIGHT; y++)
	{
		for (x = 0; x < WIDTH; x++)
		{
			if (board[y][x] == 0)
			{
				count++;
			}
		}
	}
	return count;
}

/**
 * 检查游戏是否结束
 * @param board 游戏棋盘数组
 * @return 游戏是否结束
 */
bool gameEnded(uint8_t **board)
{
	bool ended = true;
	if (countEmpty(board) > 0)
		return false;
	if (findPairDown(board))
		return false;
	
	// 检查水平方向
	for (uint8_t y = 0; y < HEIGHT; y++)
	{
		for (uint8_t x = 0; x < WIDTH - 1; x++)
		{
			if (board[y][x] == board[y][x + 1])
				return false;
		}
	}
	
	return ended;
}

/**
 * 在棋盘上随机添加一个新方块（2或4）
 * @param board 游戏棋盘数组
 */
void addRandom(uint8_t **board)
{
	static bool initialized = false;
	uint8_t x, y;
	uint8_t r, len = 0;
	uint8_t n;
	
	if (!initialized)
	{
		srand(time(NULL)); // 初始化随机数生成器
		initialized = true;
	}
	
	// 计算空位置数量
	len = countEmpty(board);
	if (len == 0)
		return;
	
	// 收集所有空位置
	uint8_t **list = (uint8_t **)malloc(len * sizeof(uint8_t *));
	for (uint8_t i = 0; i < len; i++)
	{
		list[i] = (uint8_t *)malloc(2 * sizeof(uint8_t));
	}
	
	len = 0;
	for (y = 0; y < HEIGHT; y++)
	{
		for (x = 0; x < WIDTH; x++)
		{
			if (board[y][x] == 0)
			{
				list[len][0] = y;
				list[len][1] = x;
				len++;
			}
		}
	}

	if (len > 0)
	{
		r = rand() % len; // 随机选择一个空位置
		y = list[r][0];
		x = list[r][1];
		n = (rand() % 10) / 9 + 1; // 90%的概率生成2（1），10%的概率生成4（2）
		board[y][x] = n;
	}
	
	// 释放内存
	for (uint8_t i = 0; i < len; i++)
	{
		free(list[i]);
	}
	free(list);
}

/**
 * 初始化游戏棋盘
 * @param board 游戏棋盘数组
 */
void initBoard(uint8_t **board)
{
	uint8_t x, y;
	// 清空棋盘
	for (y = 0; y < HEIGHT; y++)
	{
		for (x = 0; x < WIDTH; x++)
		{
			board[y][x] = 0;
		}
	}
	// 添加两个随机方块
	addRandom(board);
	addRandom(board);
}

/**
 * 设置终端输入模式
 * @param enable 是否启用缓冲输入
 */
void setBufferedInput(bool enable)
{
	static bool enabled = true;
	static struct termios old;
	struct termios new;

	if (enable && !enabled)
	{
		// 恢复之前的设置
		tcsetattr(STDIN_FILENO, TCSANOW, &old);
		// 设置新状态
		enabled = true;
	}
	else if (!enable && enabled)
	{
		// 获取标准输入的终端设置
		tcgetattr(STDIN_FILENO, &new);
		// 保存旧设置以便稍后恢复
		old = new;
		// 禁用规范模式（缓冲I/O）和本地回显
		new.c_lflag &= (~ICANON & ~ECHO);
		// 立即设置新设置
		tcsetattr(STDIN_FILENO, TCSANOW, &new);
		// 设置新状态
		enabled = false;
	}
}

/**
 * 测试滑动数组功能是否正常工作
 * @return 测试是否成功
 */
bool testSucceed()
{
	// 注意：测试函数仅适用于默认大小4x4的棋盘
	if (HEIGHT != 4 || WIDTH != 4)
	{
		printf("Tests only work with 4x4 board\n");
		return false;
	}
	
	uint8_t array[4];
	// 这些是2的指数（1=2 2=4 3=8）
	// 数据每行包含：4个输入，4个输出，1个得分
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
	uint32_t score;

	tests = (sizeof(data) / sizeof(data[0])) / (2 * 4 + 1);
	for (t = 0; t < tests; t++)
	{
		in = data + t * (2 * 4 + 1);
		out = in + 4;
		points = in + 2 * 4;
		for (i = 0; i < 4; i++)
		{
			array[i] = in[i];
		}
		score = 0;
		success &= slideArray(array, &score);
		for (i = 0; i < 4; i++)
		{
			if (array[i] != out[i])
			{
				success = false;
			}
		}
		if (score != *points)
		{
			success = false;
		}
		if (success == false)
		{
			for (i = 0; i < 4; i++)
			{
				printf("%u ", in[i]);
			}
			printf("=> ");
			for (i = 0; i < 4; i++)
			{
				printf("%u ", array[i]);
			}
			printf("(%u points) expected ", score);
			for (i = 0; i < 4; i++)
			{
				printf("%u ", in[i]);
			}
			printf("=> ");
			for (i = 0; i < 4; i++)
			{
				printf("%u ", out[i]);
			}
			printf("(%u points)\n", *points);
			break;
		}
	}
	if (success)
	{
		printf("All %u tests executed successfully\n", tests);
	}
	return success;
}

/**
 * 信号处理函数（当按下Ctrl+C时调用）
 * @param signum 信号编号
 */
void signal_callback_handler(int signum)
{
	printf("         TERMINATED         \n");
	setBufferedInput(true);
	// 使光标可见，重置所有模式
	printf("\033[?25h\033[m");
	exit(signum);
}

/**
 * 显示帮助信息
 * @param argv0 程序名称
 */
void showHelp(const char *argv0)
{
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

/**
 * 主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 退出状态
 */
int main(int argc, char *argv[])
{
	uint8_t scheme = 0; // 默认颜色方案
	uint32_t score = 0; // 初始得分
	int c;
	bool success;
	
	// 解析命令行参数
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"height", required_argument, 0, 'H'},
		{"width", required_argument, 0, 'W'},
		{0, 0, 0, 0}
	};
	
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "hvH:W:", long_options, &option_index)) != -1)
	{
		switch (c)
		{
		case 'h':
			showHelp(argv[0]);
			return EXIT_SUCCESS;
		case 'v':
			printf("2048.c version %s\n", VERSION);
			return EXIT_SUCCESS;
		case 'H':
			HEIGHT = atoi(optarg);
			if (HEIGHT < 2 || HEIGHT > 20)
			{
				printf("Height must be between 2 and 20\n");
				return EXIT_FAILURE;
			}
			break;
		case 'W':
			WIDTH = atoi(optarg);
			if (WIDTH < 2 || WIDTH > 20)
			{
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
	if (optind < argc)
	{
		if (strcmp(argv[optind], "bluered") == 0)
		{
			scheme = 1;
		}
		else if (strcmp(argv[optind], "blackwhite") == 0)
		{
			scheme = 2;
		}
		else if (strcmp(argv[optind], "test") == 0)
		{
			return testSucceed() ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		else
		{
			printf("Invalid mode: %s\n\nTry '%s --help' for more options.\n", argv[optind], argv[0]);
			return EXIT_FAILURE;
		}
	}

	// 动态分配棋盘内存
	uint8_t **board = (uint8_t **)malloc(HEIGHT * sizeof(uint8_t *));
	for (uint8_t i = 0; i < HEIGHT; i++)
	{
		board[i] = (uint8_t *)malloc(WIDTH * sizeof(uint8_t));
	}

	// 使光标不可见，清除整个屏幕
	printf("\033[?25l\033[2J");

	// 注册信号处理程序，当按下ctrl-c时调用
	signal(SIGINT, signal_callback_handler);

	initBoard(board); // 初始化棋盘
	setBufferedInput(false); // 设置非缓冲输入
	drawBoard(board, scheme, score); // 绘制棋盘
	while (true)
	{
		c = getchar();
		if (c == EOF)
		{
			puts("\nError! Cannot read keyboard input!");
			break;
		}
		
		// 处理方向键和其他控制键
		switch (c)
		{
		case 52:  // '4' key
		case 97:  // 'a' key
		case 104: // 'h' key
		case 68:  // left arrow
			success = moveLeft(board, &score);
			break;
		case 54:  // '6' key
		case 100: // 'd' key
		case 108: // 'l' key
		case 67:  // right arrow
			success = moveRight(board, &score);
			break;
		case 56:  // '8' key
		case 119: // 'w' key
		case 107: // 'k' key
		case 65:  // up arrow
			success = moveUp(board, &score);
			break;
		case 50:  // '2' key
		case 115: // 's' key
		case 106: // 'j' key
		case 66:  // down arrow
			success = moveDown(board, &score);
			break;
		default:
			success = false;
		}
		
		if (success)
		{
			drawBoard(board, scheme, score); // 重新绘制棋盘
			usleep(150 * 1000); // 150 ms延迟，使动画更流畅
			addRandom(board); // 添加一个新方块
			drawBoard(board, scheme, score); // 重新绘制棋盘
			if (gameEnded(board))
			{
				printf("         GAME OVER          \n");
				break;
			}
		}
		
		// 处理退出命令
		if (c == 'q')
		{
			printf("        QUIT? (y/n)         \n");
			c = getchar();
			if (c == 'y')
			{
				break;
			}
			drawBoard(board, scheme, score);
		}
		
		// 处理重新开始命令
		if (c == 'r')
		{
			printf("       RESTART? (y/n)       \n");
			c = getchar();
			if (c == 'y')
			{
				initBoard(board);
				score = 0;
			}
			drawBoard(board, scheme, score);
		}
	}
	
	setBufferedInput(true); // 恢复缓冲输入

	// 使光标可见，重置所有模式
	printf("\033[?25h\033[m");
	
	// 释放内存
	for (uint8_t i = 0; i < HEIGHT; i++)
	{
		free(board[i]);
	}
	free(board);

	return EXIT_SUCCESS;
}