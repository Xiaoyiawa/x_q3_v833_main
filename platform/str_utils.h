#ifndef PLAT_STR_UTILS_H
#define PLAT_STR_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool str_begin_with(const char * str, const char * begin, bool case_sensitivity);
bool str_end_with(const char * str, const char * begin, bool case_sensitivity);
char to_upper_case(char c);
bool is_lower_letter(char c);
bool is_upper_letter(char c);


/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
