#ifndef ZCC_TYPES_HPP
#define ZCC_TYPES_HPP

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>

namespace zcc {

// ==================== 类型系统 ====================

// 基本类型
enum class PrimitiveType {
    VOID,
    BOOL,
    I8, I16, I32, I64, I128,      // 有符号整数
    U8, U16, U32, U64, U128,      // 无符号整数
    F16, F32, F64, F80, F128,     // 浮点数
    BIGINT,                        // 高精度整数
    BIGFLOAT,                      // 高精度浮点
    STRING,                        // 字符串
    ANY,                           // 任意类型 (动态类型)
};

// 类型种类
enum class TypeKind {
    PRIMITIVE,    // 基本类型
    POINTER,      // 指针
    ARRAY,        // 数组
    FUNCTION,     // 函数
    STRUCT,       // 结构体
    CLASS,        // 类
    INTERFACE,    // 接口
    GENERIC,      // 泛型
    TUPLE,        // 元组
    OPTIONAL,     // 可选类型
    DYNAMIC,      // 动态类型
};

// 前向声明
class ZCCType;

using ZCCTypePtr = std::shared_ptr<ZCCType>;

// ZCC 类型基类
class ZCCType {
public:
    ZCCType(TypeKind kind, std::string name)
        : kind_(kind), name_(std::move(name)) {}
    
    virtual ~ZCCType() = default;
    
    TypeKind getKind() const { return kind_; }
    const std::string& getName() const { return name_; }
    
    virtual bool isPrimitive() const { return false; }
    virtual bool isPointer() const { return false; }
    virtual bool isArray() const { return false; }
    virtual bool isFunction() const { return false; }
    virtual bool isStruct() const { return false; }
    virtual bool isClass() const { return false; }
    virtual bool isGeneric() const { return false; }
    
    virtual bool equals(const ZCCType& other) const = 0;
    virtual std::string toString() const = 0;
    virtual size_t getSize() const = 0;
    virtual size_t getAlignment() const = 0;
    
    // 类型兼容性
    virtual bool isCompatibleWith(const ZCCType& other) const;
    virtual bool isSubtypeOf(const ZCCType& other) const;
    
protected:
    TypeKind kind_;
    std::string name_;
};

// ==================== 基本类型 ====================

// 基本类型
class PrimitiveTypeImpl : public ZCCType {
public:
    PrimitiveTypeImpl(PrimitiveType pt)
        : ZCCType(TypeKind::PRIMITIVE, getPrimitiveName(pt)), primitiveType_(pt) {}
    
    bool isPrimitive() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override;
    size_t getAlignment() const override;
    
    PrimitiveType getPrimitiveType() const { return primitiveType_; }
    
    static PrimitiveTypeImpl* get(PrimitiveType pt);
    
private:
    PrimitiveType primitiveType_;
    
    static std::string getPrimitiveName(PrimitiveType pt);
    static std::unordered_map<PrimitiveType, PrimitiveTypeImpl> instances_;
};

// ==================== 指针类型 ====================

// 指针类型
class PointerType : public ZCCType {
public:
    PointerType(ZCCTypePtr pointee)
        : ZCCType(TypeKind::POINTER, pointee->getName() + "*"),
          pointee_(std::move(pointee)) {}
    
    bool isPointer() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override { return sizeof(void*); }
    size_t getAlignment() const override { return sizeof(void*); }
    
    const ZCCTypePtr& getPointee() const { return pointee_; }
    
private:
    ZCCTypePtr pointee_;
};

// ==================== 数组类型 ====================

// 数组类型
class ArrayType : public ZCCType {
public:
    ArrayType(ZCCTypePtr element, size_t size)
        : ZCCType(TypeKind::ARRAY, element->getName() + "[" + std::to_string(size) + "]"),
          elementType_(std::move(element)), size_(size) {}
    
    // 动态数组
    explicit ArrayType(ZCCTypePtr element)
        : ZCCType(TypeKind::ARRAY, element->getName() + "[]"),
          elementType_(std::move(element)), size_(0) {}
    
    bool isArray() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override;
    size_t getAlignment() const override;
    
    const ZCCTypePtr& getElementType() const { return elementType_; }
    size_t getSize() const { return size_; }
    bool isDynamic() const { return size_ == 0; }
    
private:
    ZCCTypePtr elementType_;
    size_t size_;
};

// ==================== 函数类型 ====================

// 函数类型
class FunctionType : public ZCCType {
public:
    using ParamList = std::vector<std::pair<std::string, ZCCTypePtr>>;
    
    FunctionType(ZCCTypePtr returnType, ParamList params, bool variadic = false)
        : ZCCType(TypeKind::FUNCTION, buildName(returnType, params, variadic)),
          returnType_(std::move(returnType)), params_(std::move(params)),
          variadic_(variadic) {}
    
    bool isFunction() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override { return sizeof(void*); }
    size_t getAlignment() const override { return sizeof(void*); }
    
    const ZCCTypePtr& getReturnType() const { return returnType_; }
    const ParamList& getParameters() const { return params_; }
    bool isVariadic() const { return variadic_; }
    
private:
    ZCCTypePtr returnType_;
    ParamList params_;
    bool variadic_ = false;
    
    static std::string buildName(const ZCCTypePtr& ret, const ParamList& params, bool variadic);
};

// ==================== 结构体类型 ====================

// 结构体字段
struct StructField {
    std::string name;
    ZCCTypePtr type;
    size_t offset;
    size_t size;
    
    StructField(std::string n, ZCCTypePtr t, size_t off, size_t sz)
        : name(std::move(n)), type(std::move(t)), offset(off), size(sz) {}
};

// 结构体类型
class StructType : public ZCCType {
public:
    using FieldList = std::vector<StructField>;
    
    StructType(std::string name, FieldList fields)
        : ZCCType(TypeKind::STRUCT, std::move(name)), fields_(std::move(fields)) {
        calculateLayout();
    }
    
    bool isStruct() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override { return size_; }
    size_t getAlignment() const override { return alignment_; }
    
    const FieldList& getFields() const { return fields_; }
    const StructField* getField(const std::string& name) const;
    size_t getFieldIndex(const std::string& name) const;
    
private:
    FieldList fields_;
    size_t size_ = 0;
    size_t alignment_ = 0;
    
    void calculateLayout();
};

// ==================== 类类型 ====================

// 类类型
class ClassType : public ZCCType {
public:
    using MethodList = std::vector<std::pair<std::string, ZCCTypePtr>>;
    
    ClassType(std::string name, ZCCTypePtr superClass, MethodList methods)
        : ZCCType(TypeKind::CLASS, std::move(name)),
          superClass_(std::move(superClass)), methods_(std::move(methods)) {}
    
    bool isClass() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override;
    size_t getAlignment() const override;
    
    const ZCCTypePtr& getSuperClass() const { return superClass_; }
    const MethodList& getMethods() const { return methods_; }
    
    bool inheritsFrom(const ClassType& other) const;
    
private:
    ZCCTypePtr superClass_;
    MethodList methods_;
};

// ==================== 泛型类型 ====================

// 泛型参数
struct GenericParam {
    std::string name;
    std::vector<ZCCTypePtr> constraints;
    
    GenericParam(std::string n, std::vector<ZCCTypePtr> c)
        : name(std::move(n)), constraints(std::move(c)) {}
};

// 泛型类型
class GenericType : public ZCCType {
public:
    GenericType(std::string name, std::vector<GenericParam> params)
        : ZCCType(TypeKind::GENERIC, std::move(name)),
          params_(std::move(params)) {}
    
    bool isGeneric() const override { return true; }
    bool equals(const ZCCType& other) const override;
    std::string toString() const override;
    size_t getSize() const override { return 0; }  // 泛型没有固定大小
    size_t getAlignment() const override { return 0; }
    
    const std::vector<GenericParam>& getParameters() const { return params_; }
    
    // 实例化泛型
    ZCCTypePtr instantiate(const std::vector<ZCCTypePtr>& args) const;
    
private:
    std::vector<GenericParam> params_;
};

// ==================== 类型系统管理 ====================

// 类型管理器
class TypeManager {
public:
    static TypeManager& getInstance();
    
    // 获取基本类型
    ZCCTypePtr getVoid();
    ZCCTypePtr getBool();
    ZCCTypePtr getI8();
    ZCCTypePtr getI16();
    ZCCTypePtr getI32();
    ZCCTypePtr getI64();
    ZCCTypePtr getU8();
    ZCCTypePtr getU16();
    ZCCTypePtr getU32();
    ZCCTypePtr getU64();
    ZCCTypePtr getF32();
    ZCCTypePtr getF64();
    ZCCTypePtr getBigint();
    ZCCTypePtr getBigfloat();
    ZCCTypePtr getString();
    ZCCTypePtr getAny();
    
    // 创建复合类型
    ZCCTypePtr createPointer(ZCCTypePtr pointee);
    ZCCTypePtr createArray(ZCCTypePtr element, size_t size);
    ZCCTypePtr createArray(ZCCTypePtr element);  // 动态数组
    ZCCTypePtr createFunction(ZCCTypePtr returnType, 
                              FunctionType::ParamList params,
                              bool variadic = false);
    ZCCTypePtr createStruct(std::string name, StructType::FieldList fields);
    ZCCTypePtr createClass(std::string name, ZCCTypePtr superClass,
                          ClassType::MethodList methods);
    ZCCTypePtr createGeneric(std::string name, std::vector<GenericParam> params);
    
    // 类型查找
    ZCCTypePtr findType(const std::string& name);
    
private:
    TypeManager() = default;
    
    std::unordered_map<std::string, ZCCTypePtr> types_;
    std::unordered_map<std::string, ZCCTypePtr> genericInstances_;
};

// ==================== 便捷类型 ====================

inline ZCCTypePtr getVoid() { return TypeManager::getInstance().getVoid(); }
inline ZCCTypePtr getBool() { return TypeManager::getInstance().getBool(); }
inline ZCCTypePtr getI32() { return TypeManager::getInstance().getI32(); }
inline ZCCTypePtr getI64() { return TypeManager::getInstance().getI64(); }
inline ZCCTypePtr getF64() { return TypeManager::getInstance().getF64(); }
inline ZCCTypePtr getString() { return TypeManager::getInstance().getString(); }
inline ZCCTypePtr getAny() { return TypeManager::getInstance().getAny(); }
inline ZCCTypePtr getBigint() { return TypeManager::getInstance().getBigint(); }

} // namespace zcc

#endif // ZCC_TYPES_HPP
