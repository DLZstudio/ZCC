// ZCC Runtime Library - Implementation
// 完整运行时函数实现，供 CBackend 生成的 C 代码链接

#include "zcc_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// ==================== 内部工具 ====================

static void* zcc_malloc(size_t size) {
    void* p = malloc(size);
    if (!p) { fprintf(stderr, "ZCC Runtime: out of memory\n"); abort(); }
    return p;
}

static char* zcc_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)zcc_malloc(len);
    memcpy(copy, s, len);
    return copy;
}

static char* zcc_strndup(const char* s, size_t n) {
    char* copy = (char*)zcc_malloc(n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

static void zcc_int_to_str(int64_t n, char* buf, size_t bufsize) {
    snprintf(buf, bufsize, "%lld", (long long)n);
}

static void zcc_uint64_to_str(uint64_t n, char* buf, size_t bufsize) {
    snprintf(buf, bufsize, "%llu", (unsigned long long)n);
}

// ==================== ZCCValue 构造 ====================

ZCCValue zcc_null(void) {
    ZCCValue v;
    v.type = ZCC_NULL;
    v.data.int_val = 0;
    return v;
}

ZCCValue zcc_bool(bool b) {
    ZCCValue v;
    v.type = ZCC_BOOL;
    v.data.bool_val = b;
    return v;
}

ZCCValue zcc_int(int64_t i) {
    ZCCValue v;
    v.type = ZCC_INT64;
    v.data.int_val = i;
    return v;
}

ZCCValue zcc_float(double d) {
    ZCCValue v;
    v.type = ZCC_FLOAT64;
    v.data.float_val = d;
    return v;
}

ZCCValue zcc_string(const char* s) {
    ZCCValue v;
    v.type = ZCC_STRING;
    v.data.string_val = zcc_strdup(s ? s : "");
    return v;
}

ZCCValue zcc_string_len(const char* s, size_t len) {
    ZCCValue v;
    v.type = ZCC_STRING;
    v.data.string_val = zcc_strndup(s ? s : "", len);
    return v;
}

void zcc_value_free(ZCCValue* v) {
    if (!v) return;
    switch (v->type) {
    case ZCC_STRING:
        free(v->data.string_val);
        break;
    case ZCC_BIGNUM:
        bignum_free(v->data.bignum_val);
        break;
    case ZCC_ARRAY:
        zcc_array_free(v->data.array_val);
        break;
    case ZCC_OBJECT:
        zcc_object_free(v->data.object_val);
        break;
    default:
        break;
    }
    v->type = ZCC_NULL;
    v->data.int_val = 0;
}

// ==================== ZCCValue 类型判断/转换 ====================

ZCCValueType zcc_typeof(ZCCValue v) {
    return v.type;
}

bool zcc_to_bool(ZCCValue v) {
    switch (v.type) {
    case ZCC_NULL:    return false;
    case ZCC_BOOL:    return v.data.bool_val;
    case ZCC_INT64:   return v.data.int_val != 0;
    case ZCC_FLOAT64: return v.data.float_val != 0.0;
    case ZCC_STRING:  return v.data.string_val && v.data.string_val[0] != '\0';
    case ZCC_BIGNUM:  return v.data.bignum_val && v.data.bignum_val->length > 0;
    case ZCC_ARRAY:   return true;
    case ZCC_OBJECT:  return true;
    }
    return false;
}

int64_t zcc_to_int(ZCCValue v) {
    switch (v.type) {
    case ZCC_INT64:   return v.data.int_val;
    case ZCC_FLOAT64: return (int64_t)v.data.float_val;
    case ZCC_BOOL:    return v.data.bool_val ? 1 : 0;
    case ZCC_STRING:
        return v.data.string_val ? (int64_t)atoll(v.data.string_val) : 0;
    default:
        return 0;
    }
}

double zcc_to_float(ZCCValue v) {
    switch (v.type) {
    case ZCC_FLOAT64: return v.data.float_val;
    case ZCC_INT64:   return (double)v.data.int_val;
    case ZCC_BOOL:    return v.data.bool_val ? 1.0 : 0.0;
    case ZCC_STRING:
        return v.data.string_val ? atof(v.data.string_val) : 0.0;
    default:
        return 0.0;
    }
}

const char* zcc_to_cstring(ZCCValue v) {
    static char buf[256];
    switch (v.type) {
    case ZCC_NULL:    return "null";
    case ZCC_BOOL:    return v.data.bool_val ? "true" : "false";
    case ZCC_INT64:   snprintf(buf, sizeof(buf), "%lld", (long long)v.data.int_val); return buf;
    case ZCC_FLOAT64: snprintf(buf, sizeof(buf), "%g", v.data.float_val); return buf;
    case ZCC_STRING:  return v.data.string_val ? v.data.string_val : "";
    default:          return "<object>";
    }
}

char* zcc_to_string(ZCCValue v) {
    return zcc_strdup(zcc_to_cstring(v));
}

// ==================== ZCCValue 算术 ====================

ZCCValue zcc_add(ZCCValue a, ZCCValue b) {
    // 字符串拼接
    if (a.type == ZCC_STRING || b.type == ZCC_STRING) {
        const char* sa = zcc_to_cstring(a);
        const char* sb = zcc_to_cstring(b);
        char* result = (char*)zcc_malloc(strlen(sa) + strlen(sb) + 1);
        strcpy(result, sa);
        strcat(result, sb);
        ZCCValue v = zcc_string(result);
        free(result);
        return v;
    }
    // 大数
    if (a.type == ZCC_BIGNUM && b.type == ZCC_BIGNUM) {
        BigNum* r = bignum_new("0");
        bignum_add(r, a.data.bignum_val, b.data.bignum_val);
        ZCCValue v = { ZCC_BIGNUM, { .bignum_val = r } };
        return v;
    }
    // 浮点
    if (a.type == ZCC_FLOAT64 || b.type == ZCC_FLOAT64) {
        return zcc_float(zcc_to_float(a) + zcc_to_float(b));
    }
    // 整数
    return zcc_int(zcc_to_int(a) + zcc_to_int(b));
}

ZCCValue zcc_sub(ZCCValue a, ZCCValue b) {
    if (a.type == ZCC_FLOAT64 || b.type == ZCC_FLOAT64)
        return zcc_float(zcc_to_float(a) - zcc_to_float(b));
    return zcc_int(zcc_to_int(a) - zcc_to_int(b));
}

ZCCValue zcc_mul(ZCCValue a, ZCCValue b) {
    if (a.type == ZCC_FLOAT64 || b.type == ZCC_FLOAT64)
        return zcc_float(zcc_to_float(a) * zcc_to_float(b));
    return zcc_int(zcc_to_int(a) * zcc_to_int(b));
}

ZCCValue zcc_div(ZCCValue a, ZCCValue b) {
    double db = zcc_to_float(b);
    if (db == 0.0) { fprintf(stderr, "ZCC Runtime: division by zero\n"); return zcc_null(); }
    return zcc_float(zcc_to_float(a) / db);
}

ZCCValue zcc_mod(ZCCValue a, ZCCValue b) {
    int64_t ib = zcc_to_int(b);
    if (ib == 0) { fprintf(stderr, "ZCC Runtime: modulo by zero\n"); return zcc_null(); }
    return zcc_int(zcc_to_int(a) % ib);
}

ZCCValue zcc_neg(ZCCValue a) {
    if (a.type == ZCC_FLOAT64) return zcc_float(-a.data.float_val);
    return zcc_int(-zcc_to_int(a));
}

ZCCValue zcc_pow(ZCCValue a, ZCCValue b) {
    return zcc_float(pow(zcc_to_float(a), zcc_to_float(b)));
}

// ==================== ZCCValue 比较 ====================

ZCCValue zcc_eq(ZCCValue a, ZCCValue b) {
    if (a.type != b.type) return zcc_bool(false);
    switch (a.type) {
    case ZCC_NULL:  return zcc_bool(true);
    case ZCC_BOOL:  return zcc_bool(a.data.bool_val == b.data.bool_val);
    case ZCC_INT64: return zcc_bool(a.data.int_val == b.data.int_val);
    case ZCC_FLOAT64: return zcc_bool(a.data.float_val == b.data.float_val);
    case ZCC_STRING:
        return zcc_bool(strcmp(a.data.string_val ? a.data.string_val : "",
                               b.data.string_val ? b.data.string_val : "") == 0);
    default: return zcc_bool(false);
    }
}

ZCCValue zcc_ne(ZCCValue a, ZCCValue b) { return zcc_bool(!zcc_to_bool(zcc_eq(a, b))); }
ZCCValue zcc_lt(ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_float(a) <  zcc_to_float(b)); }
ZCCValue zcc_le(ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_float(a) <= zcc_to_float(b)); }
ZCCValue zcc_gt(ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_float(a) >  zcc_to_float(b)); }
ZCCValue zcc_ge(ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_float(a) >= zcc_to_float(b)); }

ZCCValue zcc_and(ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_bool(a) && zcc_to_bool(b)); }
ZCCValue zcc_or (ZCCValue a, ZCCValue b) { return zcc_bool(zcc_to_bool(a) || zcc_to_bool(b)); }
ZCCValue zcc_not(ZCCValue a)             { return zcc_bool(!zcc_to_bool(a)); }

// ==================== BigNum 字符串高精度算术 ====================

// 反转字符串
static void str_rev(char* s, int len) {
    for (int i = 0; i < len / 2; i++) {
        char t = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = t;
    }
}

// 去除前导零
static void strip_leading_zeros(char* s, int* len) {
    while (*len > 1 && s[*len - 1] == '0') (*len)--;
    s[*len] = '\0';
}

BigNum* bignum_new(const char* digits) {
    BigNum* bn = (BigNum*)zcc_malloc(sizeof(BigNum));
    bn->negative = false;
    
    const char* start = digits;
    if (*start == '-') { bn->negative = true; start++; }
    else if (*start == '+') start++;
    
    // 跳过前导零
    while (*start == '0' && *(start + 1) != '\0') start++;
    
    bn->length = (int)strlen(start);
    bn->digits = (char*)zcc_malloc(bn->length + 1);
    strcpy(bn->digits, start);
    str_rev(bn->digits, bn->length);  // 反转以便计算
    
    return bn;
}

BigNum* bignum_from_int(int64_t n) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    return bignum_new(buf);
}

void bignum_free(BigNum* bn) {
    if (bn) { free(bn->digits); free(bn); }
}

char* bignum_to_string(BigNum* bn) {
    // 反转回来
    str_rev(bn->digits, bn->length);
    
    int out_len = bn->length + (bn->negative ? 1 : 0);
    char* result = (char*)zcc_malloc(out_len + 1);
    if (bn->negative) { result[0] = '-'; strcpy(result + 1, bn->digits); }
    else { strcpy(result, bn->digits); }
    
    // 恢复反转
    str_rev(bn->digits, bn->length);
    return result;
}

// 加法: result = a + b (支持任意长度)
void bignum_add(BigNum* result, const BigNum* a, const BigNum* b) {
    if (a->negative == b->negative) {
        // 同号: 绝对值相加
        int max_len = (a->length > b->length ? a->length : b->length) + 2;
        char* sum = (char*)zcc_malloc(max_len);
        int carry = 0, len = 0;
        
        for (int i = 0; i < max_len - 1; i++) {
            int da = (i < a->length) ? (a->digits[i] - '0') : 0;
            int db = (i < b->length) ? (b->digits[i] - '0') : 0;
            int s = da + db + carry;
            sum[len++] = '0' + (s % 10);
            carry = s / 10;
        }
        if (carry) sum[len++] = '1';
        sum[len] = '\0';
        
        free(result->digits);
        result->digits = sum;
        result->length = len;
        result->negative = a->negative;
        strip_leading_zeros(result->digits, &result->length);
    } else {
        // 异号: 绝对值相减
        const BigNum* larger = a;
        const BigNum* smaller = b;
        bool result_neg = a->negative;
        
        // 比较绝对值
        if (a->length < b->length || (a->length == b->length && 
            a->digits[a->length-1] < b->digits[b->length-1])) {
            larger = b; smaller = a;
            result_neg = b->negative;
        }
        
        int max_len = larger->length + 1;
        char* diff = (char*)zcc_malloc(max_len);
        int borrow = 0, len = 0;
        
        for (int i = 0; i < larger->length; i++) {
            int dl = larger->digits[i] - '0';
            int ds = (i < smaller->length) ? (smaller->digits[i] - '0') : 0;
            int d = dl - ds - borrow;
            if (d < 0) { d += 10; borrow = 1; }
            else borrow = 0;
            diff[len++] = '0' + d;
        }
        diff[len] = '\0';
        
        free(result->digits);
        result->digits = diff;
        result->length = len;
        result->negative = result_neg;
        strip_leading_zeros(result->digits, &result->length);
        if (result->length == 1 && result->digits[0] == '0')
            result->negative = false;
    }
}

void bignum_sub(BigNum* result, const BigNum* a, const BigNum* b) {
    // a - b = a + (-b)
    BigNum neg_b = *b;
    neg_b.negative = !b->negative;
    
    char* neg_b_copy = zcc_strdup(b->digits);
    neg_b.digits = neg_b_copy;
    
    bignum_add(result, a, &neg_b);
    free(neg_b_copy);
}

// 乘法 (小学竖式)
void bignum_mul(BigNum* result, const BigNum* a, const BigNum* b) {
    int max_len = a->length + b->length + 1;
    char* product = (char*)zcc_malloc(max_len);
    memset(product, 0, max_len);
    
    for (int i = 0; i < a->length; i++) {
        int carry = 0;
        for (int j = 0; j < b->length; j++) {
            int prod = (a->digits[i] - '0') * (b->digits[j] - '0') + carry;
            if (product[i + j] == 0) product[i + j] = '0';
            prod += product[i + j] - '0';
            product[i + j] = '0' + (prod % 10);
            carry = prod / 10;
        }
        if (carry) {
            if (product[i + b->length] == 0) product[i + b->length] = '0';
            int idx = i + b->length;
            int val = (product[idx] - '0') + carry;
            product[idx] = '0' + (val % 10);
        }
    }
    
    int len = a->length + b->length;
    while (len > 0 && product[len - 1] == 0) len--;
    if (len == 0) { product[0] = '0'; len = 1; }
    product[len] = '\0';
    
    free(result->digits);
    result->digits = product;
    result->length = len;
    result->negative = (a->negative != b->negative);
    if (len == 1 && product[0] == '0') result->negative = false;
}

// 除法 (简单实现：长除法)
void bignum_div(BigNum* result, const BigNum* a, const BigNum* b) {
    // 简化为整数除法
    int64_t ia = 0, ib = 0;
    char* sa = bignum_to_string((BigNum*)a);
    char* sb = bignum_to_string((BigNum*)b);
    str_rev(a->digits, a->length);  // 恢复
    str_rev(b->digits, b->length);
    
    ia = (int64_t)atoll(sa);
    ib = (int64_t)atoll(sb);
    free(sa); free(sb);
    
    if (ib == 0) { fprintf(stderr, "ZCC Runtime: division by zero\n"); return; }
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)(ia / ib));
    BigNum* temp = bignum_new(buf);
    free(result->digits);
    result->digits = temp->digits;
    result->length = temp->length;
    result->negative = (a->negative != b->negative);
    free(temp);
    
    str_rev(a->digits, a->length);
    str_rev(b->digits, b->length);
}

void bignum_mod(BigNum* result, const BigNum* a, const BigNum* b) {
    int64_t ia = 0, ib = 0;
    char* sa = bignum_to_string((BigNum*)a);
    char* sb = bignum_to_string((BigNum*)b);
    str_rev(a->digits, a->length);
    str_rev(b->digits, b->length);
    
    ia = (int64_t)atoll(sa);
    ib = (int64_t)atoll(sb);
    free(sa); free(sb);
    
    if (ib == 0) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)(ia % ib));
    BigNum* temp = bignum_new(buf);
    free(result->digits);
    result->digits = temp->digits;
    result->length = temp->length;
    result->negative = a->negative;
    free(temp);
    
    str_rev(a->digits, a->length);
    str_rev(b->digits, b->length);
}

int bignum_cmp(const BigNum* a, const BigNum* b) {
    if (a->negative != b->negative)
        return a->negative ? -1 : 1;
    
    int sign = a->negative ? -1 : 1;
    if (a->length != b->length)
        return sign * (a->length > b->length ? 1 : -1);
    
    for (int i = a->length - 1; i >= 0; i--) {
        if (a->digits[i] != b->digits[i])
            return sign * (a->digits[i] > b->digits[i] ? 1 : -1);
    }
    return 0;
}

void bignum_pow(BigNum* result, const BigNum* base, const BigNum* exp) {
    int64_t exp_val = 0;
    char* sexp = bignum_to_string((BigNum*)exp);
    exp_val = (int64_t)atoll(sexp);
    free(sexp);
    str_rev(exp->digits, exp->length);
    
    if (exp_val == 0) {
        free(result->digits);
        result->digits = zcc_strdup("1");
        result->length = 1;
        result->negative = false;
        str_rev(result->digits, result->length);
        return;
    }
    
    // 复制 base 到 result
    free(result->digits);
    result->digits = (char*)zcc_malloc(base->length + 1);
    memcpy(result->digits, base->digits, base->length);
    result->digits[base->length] = '\0';
    result->length = base->length;
    result->negative = base->negative && (exp_val % 2 == 1);
    
    BigNum one;
    one.digits = zcc_strdup("1");
    one.length = 1;
    one.negative = false;
    str_rev(one.digits, one.length);
    
    BigNum temp_res;
    temp_res.digits = NULL;
    temp_res.length = 0;
    
    for (int64_t i = 1; i < exp_val; i++) {
        BigNum current = *result;
        current.digits = (char*)zcc_malloc(result->length + 1);
        memcpy(current.digits, result->digits, result->length);
        current.digits[result->length] = '\0';
        
        bignum_mul(&temp_res, &current, base);
        free(current.digits);
        
        free(result->digits);
        result->digits = temp_res.digits;
        result->length = temp_res.length;
        result->negative = temp_res.negative;
        
        temp_res.digits = NULL;
        temp_res.length = 0;
    }
    
    free(one.digits);
}

void bignum_negate(BigNum* result, const BigNum* a) {
    free(result->digits);
    result->digits = (char*)zcc_malloc(a->length + 1);
    memcpy(result->digits, a->digits, a->length);
    result->digits[a->length] = '\0';
    result->length = a->length;
    result->negative = !a->negative;
}

// ==================== 数组 ====================

ZCCArray* zcc_array_new(void) {
    ZCCArray* arr = (ZCCArray*)zcc_malloc(sizeof(ZCCArray));
    arr->capacity = 8;
    arr->length = 0;
    arr->data = (ZCCValue*)zcc_malloc(sizeof(ZCCValue) * arr->capacity);
    return arr;
}

void zcc_array_free(ZCCArray* arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->length; i++)
        zcc_value_free(&arr->data[i]);
    free(arr->data);
    free(arr);
}

ZCCValue zcc_array_get(ZCCArray* arr, int64_t idx) {
    if (!arr || idx < 0 || (size_t)idx >= arr->length)
        return zcc_null();
    return arr->data[idx];
}

void zcc_array_set(ZCCArray* arr, int64_t idx, ZCCValue val) {
    if (!arr || idx < 0) return;
    
    // 自动扩容
    while ((size_t)idx >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = (ZCCValue*)realloc(arr->data, sizeof(ZCCValue) * arr->capacity);
    }
    
    // 初始化空洞为 null
    while (arr->length <= (size_t)idx) {
        arr->data[arr->length++] = zcc_null();
    }
    
    zcc_value_free(&arr->data[idx]);
    arr->data[idx] = val;
}

int64_t zcc_array_len(ZCCArray* arr) {
    return arr ? (int64_t)arr->length : 0;
}

void zcc_array_push(ZCCArray* arr, ZCCValue val) {
    if (!arr) return;
    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = (ZCCValue*)realloc(arr->data, sizeof(ZCCValue) * arr->capacity);
    }
    arr->data[arr->length++] = val;
}

// ==================== 对象 (哈希表) ====================

static unsigned int object_hash(const char* key) {
    unsigned int hash = 5381;
    while (*key) hash = ((hash << 5) + hash) + (unsigned char)*key++;
    return hash % ZCC_OBJECT_BUCKETS;
}

ZCCObject* zcc_object_new(void) {
    ZCCObject* obj = (ZCCObject*)zcc_malloc(sizeof(ZCCObject));
    memset(obj->buckets, 0, sizeof(obj->buckets));
    obj->count = 0;
    return obj;
}

void zcc_object_free(ZCCObject* obj) {
    if (!obj) return;
    for (int i = 0; i < ZCC_OBJECT_BUCKETS; i++) {
        ZCCObjectEntry* entry = obj->buckets[i];
        while (entry) {
            ZCCObjectEntry* next = entry->next;
            free(entry->key);
            zcc_value_free(&entry->value);
            free(entry);
            entry = next;
        }
    }
    free(obj);
}

ZCCValue zcc_object_get(ZCCObject* obj, const char* key) {
    if (!obj || !key) return zcc_null();
    
    unsigned int bucket = object_hash(key);
    ZCCObjectEntry* entry = obj->buckets[bucket];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return entry->value;
        entry = entry->next;
    }
    return zcc_null();
}

void zcc_object_set(ZCCObject* obj, const char* key, ZCCValue val) {
    if (!obj || !key) return;
    
    unsigned int bucket = object_hash(key);
    ZCCObjectEntry* entry = obj->buckets[bucket];
    
    // 查找已存在的 key
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            zcc_value_free(&entry->value);
            entry->value = val;
            return;
        }
        entry = entry->next;
    }
    
    // 新建 entry
    ZCCObjectEntry* new_entry = (ZCCObjectEntry*)zcc_malloc(sizeof(ZCCObjectEntry));
    new_entry->key = zcc_strdup(key);
    new_entry->value = val;
    new_entry->next = obj->buckets[bucket];
    obj->buckets[bucket] = new_entry;
    obj->count++;
}

bool zcc_object_has(ZCCObject* obj, const char* key) {
    if (!obj || !key) return false;
    
    unsigned int bucket = object_hash(key);
    ZCCObjectEntry* entry = obj->buckets[bucket];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) return true;
        entry = entry->next;
    }
    return false;
}

// ==================== 字符串工具 ====================

char* str_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t len = strlen(a) + strlen(b) + 1;
    char* result = (char*)zcc_malloc(len);
    strcpy(result, a);
    strcat(result, b);
    return result;
}

char* str_slice(const char* s, int64_t start, int64_t end) {
    if (!s) return zcc_strdup("");
    int64_t slen = (int64_t)strlen(s);
    if (start < 0) start = slen + start;
    if (end < 0) end = slen + end;
    if (start < 0) start = 0;
    if (end > slen) end = slen;
    if (start >= end) return zcc_strdup("");
    return zcc_strndup(s + start, (size_t)(end - start));
}

// ==================== 异常 ====================

void zcc_throw(ZCCValue ex) {
    fprintf(stderr, "ZCC Runtime Exception: %s\n", zcc_to_cstring(ex));
    zcc_value_free(&ex);
    abort();
}

// ==================== 打印 ====================

void zcc_print(ZCCValue v) {
    printf("%s", zcc_to_cstring(v));
}

void zcc_println(ZCCValue v) {
    printf("%s\n", zcc_to_cstring(v));
}

void zcc_print_str(const char* s) {
    printf("%s", s ? s : "");
}
