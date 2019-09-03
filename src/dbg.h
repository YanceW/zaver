
/*
 * Copyright (C) Zhu Jiashun
 * Copyright (C) Zaver
 */

#ifndef DBG_H
#define DBG_H

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef NDEBUG
#define debug(M, ...)
#else

/*

#define P(A) printf("%s:%d\n",#A //#运算符把参数变字符串,A);
int main(int argc, char **argv)
{
        int a = 1, b = 2;
        P(a);
        P(b);
        P(a+b);
        system("pause");
}







##:在宏定义中将两个字符连接起来，构成一个新的标识符,粘合剂

#define SETTEST(N) test_ ## N 
在使用时： int SETTEST(1)=2; 其实就相当于 int test_1=2;
*/
/*
	编译器内置宏
	__FILE__:当前文件绝对路径
	__LINE__:__LINE__所在文件的当前行号
	__DATE__:当前编译日期
	__TIME__:当前编译时间
	__func__:当前函数名
	__cplusplus: cpp文件，改标识符被定义
	__VA_ARGS__：可变参数宏，...的内容
	##__VA_ARGS__：去掉没有参数时前面的，（不明白）
*/
#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

// M: msg
// A: ? 条件
#define log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) { log_err(M "\n", ##__VA_ARGS__); /* exit(1); */ }

#define check_exit(A, M, ...) if(!(A)) { log_err(M "\n", ##__VA_ARGS__); exit(1);}

#define check_debug(A, M, ...) if(!(A)) { debug(M "\n", ##__VA_ARGS__); /* exit(1); */}

#endif
