#ifndef PROJ_PAGE_MANAGER_H
#define PROJ_PAGE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"
#include "../lv_lib_100ask/lv_lib_100ask.h"
#include <stdio.h>
#include <stdlib.h>

/*********************
 *      DEFINES
 *********************/
#define MAX_PAGE_STACK 32 // 最大页面堆栈深度

/**********************
 *      TYPEDEFS
 **********************/

typedef struct
{
    lv_obj_t * obj;   // 页面对象
    void * user_data; // 用户数据
    void (*on_create)(void *);
    void (*on_resume)(void *);
    void (*on_pause)(void *);
    void (*on_destroy)(void *);

} BasePage;

typedef struct
{
    BasePage * stack[MAX_PAGE_STACK]; // 页面堆栈
    int top;                        // 栈顶指针
} PageManager;


/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * 创建一个基础页面
 */
BasePage * base_page_create(lv_obj_t * obj, void (*on_create)(void *), void (*on_destroy)(void *));

/**
 * 初始化，没什么好说的，初次使用时调用即可
 */
void page_manager_init(void);

/**
 * 旧版本，用于兼容旧版本的页面
 * 也可以用于极简页面
 */
void page_open_obj(lv_obj_t * obj);

/**
 * 传入一个BasePage指针并显示它作为页面，之前的页面放入堆栈中（并进行on_pause回调）
 * 如何创建自己的页面类型？见base_page_create()
 * 或者obj那个用于简单页面也是可以的
 */
void page_open(BasePage * new_page);

/**
 * 销毁当前页面并返回上一页
 */
void page_back(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
