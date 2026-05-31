// ZCC Runtime Library - Header
// 为 CBackend 生成的 C 代码提供所有运行时类型和函数
// 支持: 动态类型系统、高精度算术、数组、对象、异常
#ifndef ZCC_RUNTIME_H
#define ZCC_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型标签 ====================
typedef enum {
    ZCC_NULL = 0,
    ZCC_BOOL,
    ZCC_INT64,
    ZCC_FLOAT64,
    ZCC_STRING,
    ZCC_BIGNUM,
    ZCC_ARRAY,
    ZCC_OBJECT
} ZCCValueType;

// ==================== 大数 (BigNum) ====================
typedef struct {
    char* digits;       // 数字字符串 (malloc 分配)
    int   length;       // 字符串长度
    bool  negative;     // 是否为负数
} BigNum;

// ==================== 前向声明 ====================
typedef struct ZCCArray  ZCCArray;
typedef struct ZCCObject ZCCObject;

// ==================== ZCCValue (动态类型值) ====================
typedef struct {
    ZCCValueType type;
    union {
        bool     bool_val;
        int64_t  int_val;
        double   float_val;
        char*    string_val;    // malloc 分配
        BigNum*  bignum_val;    // malloc 分配
        ZCCArray*  array_val;
        ZCCObject* object_val;
    } data;
} ZCCValue;

// ==================== 值构造/析构 ====================
ZCCValue zcc_null(void);
ZCCValue zcc_bool(bool b);
ZCCValue zcc_int(int64_t i);
ZCCValue zcc_float(double d);
ZCCValue zcc_string(const char* s);
ZCCValue zcc_string_len(const char* s, size_t len);
void     zcc_value_free(ZCCValue* v);        // 释放内部内存

// 类型判断和转换
ZCCValueType zcc_typeof(ZCCValue v);
bool         zcc_to_bool(ZCCValue v);
int64_t      zcc_to_int(ZCCValue v);
double       zcc_to_float(ZCCValue v);
const char*  zcc_to_cstring(ZCCValue v);
char*        zcc_to_string(ZCCValue v);       // 返回 malloc 的字符串表示

// 算术运算 (动态类型 => 结果也是 ZCCValue)
ZCCValue zcc_add(ZCCValue a, ZCCValue b);
ZCCValue zcc_sub(ZCCValue a, ZCCValue b);
ZCCValue zcc_mul(ZCCValue a, ZCCValue b);
ZCCValue zcc_div(ZCCValue a, ZCCValue b);
ZCCValue zcc_mod(ZCCValue a, ZCCValue b);
ZCCValue zcc_neg(ZCCValue a);
ZCCValue zcc_pow(ZCCValue a, ZCCValue b);

// 比较 (返回 ZCC_BOOL)
ZCCValue zcc_eq (ZCCValue a, ZCCValue b);
ZCCValue zcc_ne (ZCCValue a, ZCCValue b);
ZCCValue zcc_lt (ZCCValue a, ZCCValue b);
ZCCValue zcc_le (ZCCValue a, ZCCValue b);
ZCCValue zcc_gt (ZCCValue a, ZCCValue b);
ZCCValue zcc_ge (ZCCValue a, ZCCValue b);

// 逻辑
ZCCValue zcc_and(ZCCValue a, ZCCValue b);
ZCCValue zcc_or (ZCCValue a, ZCCValue b);
ZCCValue zcc_not(ZCCValue a);

// ==================== 高精度大数 (BigNum) ====================
BigNum* bignum_new(const char* digits);
BigNum* bignum_from_int(int64_t n);
void    bignum_free(BigNum* bn);
char*   bignum_to_string(BigNum* bn);  // 返回 malloc 字符串

void bignum_add    (BigNum* result, const BigNum* a, const BigNum* b);
void bignum_sub    (BigNum* result, const BigNum* a, const BigNum* b);
void bignum_mul    (BigNum* result, const BigNum* a, const BigNum* b);
void bignum_div    (BigNum* result, const BigNum* a, const BigNum* b);
void bignum_mod    (BigNum* result, const BigNum* a, const BigNum* b);
int  bignum_cmp    (const BigNum* a, const BigNum* b);  // -1/0/1
void bignum_pow    (BigNum* result, const BigNum* base, const BigNum* exp);
void bignum_negate (BigNum* result, const BigNum* a);

// ==================== 数组 ====================
struct ZCCArray {
    ZCCValue* data;
    size_t    capacity;
    size_t    length;
};

ZCCArray* zcc_array_new(void);
void      zcc_array_free(ZCCArray* arr);
ZCCValue  zcc_array_get(ZCCArray* arr, int64_t idx);
void      zcc_array_set(ZCCArray* arr, int64_t idx, ZCCValue val);
int64_t   zcc_array_len(ZCCArray* arr);
void      zcc_array_push(ZCCArray* arr, ZCCValue val);

// ==================== 对象 ====================
typedef struct ZCCObjectEntry {
    char*    key;
    ZCCValue value;
    struct ZCCObjectEntry* next;  // 链表解决哈希冲突
} ZCCObjectEntry;

#define ZCC_OBJECT_BUCKETS 64

struct ZCCObject {
    ZCCObjectEntry* buckets[ZCC_OBJECT_BUCKETS];
    size_t          count;
};

ZCCObject* zcc_object_new(void);
void       zcc_object_free(ZCCObject* obj);
ZCCValue   zcc_object_get(ZCCObject* obj, const char* key);
void       zcc_object_set(ZCCObject* obj, const char* key, ZCCValue val);
bool       zcc_object_has(ZCCObject* obj, const char* key);

// ==================== 字符串 ====================
char* str_concat(const char* a, const char* b);   // 返回 malloc 字符串
char* str_slice (const char* s, int64_t start, int64_t end);

// ==================== 异常 ====================
void zcc_throw(ZCCValue ex);  // 简单实现：打印并 abort()
void zcc_try_begin(void);
bool zcc_try_catch(ZCCValue* out_ex);

// ==================== 打印 (调试) ====================
void zcc_print    (ZCCValue v);
void zcc_println  (ZCCValue v);
void zcc_print_str(const char* s);

#ifdef __cplusplus
}
#endif

#endif // ZCC_RUNTIME_H
