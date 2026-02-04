// page_totp.c
#include "page_totp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

// 全局变量
static lv_obj_t *screen = NULL;
static lv_obj_t *code_label = NULL;
static lv_obj_t *timer_label = NULL;
static lv_timer_t *update_timer = NULL;
static uint8_t totp_key[64] = {0};
static size_t key_len = 0;
static int has_key = 0;

// 函数声明
static void back_click(lv_event_t *e);
static void update_totp(lv_timer_t *timer);
static uint32_t generate_totp(const uint8_t *key, size_t key_len, uint64_t time);
static void hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *result);
static void sha1(const uint8_t *data, size_t len, uint8_t *hash);
static int base32_decode(const char *encoded, uint8_t *result, size_t *result_len);
static uint32_t sha1_rol32(uint32_t value, uint32_t shift);
static void sha1_process_block(uint32_t state[5], const uint8_t block[64]);

// Base32字母表
static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// ==================== Base32解码 ====================
static int base32_decode(const char *encoded, uint8_t *result, size_t *result_len) {
    if (!encoded || !result || !result_len) return -1;
    
    size_t input_len = strlen(encoded);
    size_t output_idx = 0;
    uint32_t buffer = 0;
    int bits = 0;
    
    for (size_t i = 0; i < input_len; i++) {
        unsigned char c = encoded[i];
        
        // 跳过填充字符和空白
        if (c == '=' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        
        // 查找字符在Base32字母表中的位置
        const char *pos = NULL;
        for (int j = 0; j < 32; j++) {
            if (base32_alphabet[j] == c) {
                pos = &base32_alphabet[j];
                break;
            }
        }
        
        // 尝试小写字母
        if (!pos && c >= 'a' && c <= 'z') {
            c = c - 32; // 转换为大写
            for (int j = 0; j < 32; j++) {
                if (base32_alphabet[j] == c) {
                    pos = &base32_alphabet[j];
                    break;
                }
            }
        }
        
        if (!pos) {
            return -1; // 非法字符
        }
        
        uint8_t value = pos - base32_alphabet;
        
        // 将5位值添加到缓冲区
        buffer = (buffer << 5) | value;
        bits += 5;
        
        // 当有至少8位时，输出一个字节
        if (bits >= 8) {
            bits -= 8;
            if (output_idx < *result_len) {
                result[output_idx++] = (buffer >> bits) & 0xFF;
            } else {
                return -1; // 输出缓冲区不足
            }
        }
    }
    
    *result_len = output_idx;
    return 0;
}

// ==================== SHA1辅助函数 ====================
static uint32_t sha1_rol32(uint32_t value, uint32_t shift) {
    return (value << shift) | (value >> (32 - shift));
}

static void sha1_process_block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    int i;
    
    // 初始化前16个字
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    
    // 扩展
    for (i = 16; i < 80; i++) {
        w[i] = sha1_rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }
    
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    
    // 主循环
    for (i = 0; i < 80; i++) {
        uint32_t f, k;
        
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        
        uint32_t temp = sha1_rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol32(b, 30);
        b = a;
        a = temp;
    }
    
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

// ==================== SHA1主函数 ====================
static void sha1(const uint8_t *data, size_t len, uint8_t *hash) {
    uint32_t state[5] = {
        0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
    };
    
    uint64_t bit_len = len * 8;
    uint8_t buffer[64];
    size_t i, index = 0;
    
    // 处理完整块
    for (i = 0; i + 63 < len; i += 64) {
        sha1_process_block(state, data + i);
    }
    
    // 处理最后的不完整块
    index = len % 64;
    memcpy(buffer, data + i, index);
    
    // 添加填充位
    buffer[index++] = 0x80;
    
    // 如果当前块剩余空间不足8字节存放长度
    if (index > 56) {
        while (index < 64) buffer[index++] = 0;
        sha1_process_block(state, buffer);
        index = 0;
    }
    
    // 填充0直到剩余56字节
    while (index < 56) buffer[index++] = 0;
    
    // 添加长度（大端序，64位）
    for (i = 63; i >= 56; i--) {
        buffer[i] = bit_len & 0xFF;
        bit_len >>= 8;
    }
    
    sha1_process_block(state, buffer);
    
    // 输出哈希值（大端序）
    for (i = 0; i < 5; i++) {
        hash[i * 4] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
}

// ==================== HMAC-SHA1 ====================
static void hmac_sha1(const uint8_t *key, size_t key_len, 
                      const uint8_t *data, size_t data_len, 
                      uint8_t *result) {
    uint8_t k_ipad[64] = {0};
    uint8_t k_opad[64] = {0};
    uint8_t hash1[20];
    
    // 如果密钥长度大于64字节，先对其进行SHA1哈希
    if (key_len > 64) {
        sha1(key, key_len, k_ipad);
        key = k_ipad;
        key_len = 20;
    }
    
    // 准备内填充和外填充密钥
    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);
    
    // 与标准填充值进行异或
    for (int i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5C;
    }
    
    // 计算内哈希: H((K XOR ipad) || text)
    uint8_t inner_data[128]; // 64 + 最大可能的数据长度
    memcpy(inner_data, k_ipad, 64);
    memcpy(inner_data + 64, data, data_len);
    sha1(inner_data, 64 + data_len, hash1);
    
    // 计算外哈希: H((K XOR opad) || inner_hash)
    uint8_t outer_data[84]; // 64 + 20
    memcpy(outer_data, k_opad, 64);
    memcpy(outer_data + 64, hash1, 20);
    sha1(outer_data, 84, result);
}

// ==================== TOTP生成 ====================
static uint32_t generate_totp(const uint8_t *key, size_t key_len, uint64_t time) {
    // TOTP时间步长（30秒）
    uint64_t time_step = time / 30;
    
    // 将时间步长转换为8字节的大端序数组
    uint8_t time_bytes[8];
    for (int i = 7; i >= 0; i--) {
        time_bytes[i] = (uint8_t)(time_step & 0xFF);
        time_step >>= 8;
    }
    
    // 计算HMAC-SHA1
    uint8_t hmac_result[20];
    hmac_sha1(key, key_len, time_bytes, 8, hmac_result);
    
    // 动态截取偏移量 (RFC 4226)
    int offset = hmac_result[19] & 0x0F;
    
    // 提取4字节动态值，忽略最高位
    uint32_t binary = 
        ((hmac_result[offset] & 0x7F) << 24) |
        ((hmac_result[offset + 1] & 0xFF) << 16) |
        ((hmac_result[offset + 2] & 0xFF) << 8) |
        (hmac_result[offset + 3] & 0xFF);
    
    // 生成6位数字
    return binary % 1000000;
}

// ==================== 主函数 ====================
lv_obj_t * page_totp(void) {
    // 创建屏幕
    screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, 240, 240);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    
    // 修改点1：更新密钥文件路径
    const char *key_path = "/mnt/UDISK/key.txt";
    FILE *fp = fopen(key_path, "r");
    
    has_key = 0;
    key_len = 0;
    memset(totp_key, 0, sizeof(totp_key));
    
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            // 移除可能的换行符和空格
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || 
                               line[len-1] == ' ' || line[len-1] == '\t' || 
                               line[len-1] == '=')) {
                line[len-1] = '\0';
                len--;
            }
            
            if (len > 0) {
                // Base32解码
                size_t max_result_len = sizeof(totp_key);
                int decode_result = base32_decode(line, totp_key, &max_result_len);
                if (decode_result == 0 && max_result_len > 0) {
                    key_len = max_result_len;
                    has_key = 1;
                }
            }
        }
        fclose(fp);
    }
    
    if (has_key) {
        // 有密钥时：正常显示TOTP界面
        // 创建六位数字显示（中间）
        code_label = lv_label_create(screen);
        lv_label_set_text(code_label, "------");
        lv_obj_set_style_text_color(code_label, lv_color_make(0, 0, 0), 0);
        
        // 设置字体
        lv_obj_set_style_text_font(code_label, &lv_font_montserrat_48, 0);
        lv_obj_align(code_label, LV_ALIGN_CENTER, 0, 0);
        
        // 创建倒计时显示（底部中间，向上6像素）
        timer_label = lv_label_create(screen);
        lv_label_set_text(timer_label, "30");
        // 修改点2：计时器字体改为16像素
        lv_obj_set_style_text_font(timer_label, &lv_font_montserrat_16, 0);
        lv_obj_align(timer_label, LV_ALIGN_BOTTOM_MID, 0, -6);
        
    } else {
        // 修改点3：没有密钥时显示提示信息
        // 显示大标题 "No Secret Key"
        lv_obj_t *error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "No Secret Key");
        lv_obj_set_style_text_color(error_label, lv_color_make(255, 0, 0), 0);
        lv_obj_set_style_text_font(error_label, &lv_font_montserrat_28, 0); // 保持大字体
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, -20); // 向上偏移一点
        
        // 显示小字路径提示 "Location /mnt/UDISK/key.txt"
        lv_obj_t *path_label = lv_label_create(screen);
        char path_text[60];
        snprintf(path_text, sizeof(path_text), "Location %s", key_path);
        lv_label_set_text(path_label, path_text);
        lv_obj_set_style_text_color(path_label, lv_color_make(100, 100, 100), 0); // 灰色
        lv_obj_set_style_text_font(path_label, &lv_font_montserrat_16, 0); // 小字体
        lv_obj_align(path_label, LV_ALIGN_CENTER, 0, 20); // 在大标题下方
        
        // 没有密钥时不需要创建 code_label 和 timer_label
        code_label = NULL;
        timer_label = NULL;
    }
    
    // Back按钮（左下角，始终显示）
    lv_obj_t *btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, "back");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
    
    // 只有检测到密钥时才创建更新计时器
    if (has_key) {
        update_timer = lv_timer_create(update_totp, 1000, NULL);
        update_totp(update_timer); // 立即更新一次
    } else {
        update_timer = NULL; // 确保计时器指针为NULL
    }
    
    return screen;
}

// ==================== 更新TOTP显示 ====================
static void update_totp(lv_timer_t *timer) {
    (void)timer; // 未使用参数
    
    if (!has_key || !code_label || !timer_label) return;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // 获取当前时间戳（秒）
    uint64_t timestamp = (uint64_t)tv.tv_sec;
    
    // 生成TOTP
    uint32_t code = generate_totp(totp_key, key_len, timestamp);
    
    // 更新六位数字显示
    char code_str[7];
    
    // 手动转换数字为字符串
    uint32_t temp = code;
    code_str[6] = '\0';
    for (int i = 5; i >= 0; i--) {
        code_str[i] = '0' + (temp % 10);
        temp /= 10;
    }
    
    lv_label_set_text(code_label, code_str);
    
    // 更新倒计时（30秒周期）
    int remaining = 30 - (tv.tv_sec % 30);
    char time_str[10];
    
    // 手动转换倒计时为字符串
    if (remaining >= 10) {
        time_str[0] = '0' + (remaining / 10);
        time_str[1] = '0' + (remaining % 10);
        time_str[2] = '\0';
    } else {
        time_str[0] = '0' + remaining;
        time_str[1] = '\0';
    }
    
    lv_label_set_text(timer_label, time_str);
    
    // 如果只剩5秒，变为红色提醒
    if (remaining <= 5) {
        lv_obj_set_style_text_color(timer_label, lv_color_make(255, 0, 0), 0);
    } else {
        lv_obj_set_style_text_color(timer_label, lv_color_make(0, 0, 0), 0);
    }
}

// ==================== Back按钮点击事件 ====================
static void back_click(lv_event_t *e) {
    // 重要：销毁计时器以防止内存泄漏
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    // 清理并返回
    page_back();
}