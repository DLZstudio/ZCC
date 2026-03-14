#ifndef ZCC_SYMBOLTABLE_HPP
#define ZCC_SYMBOLTABLE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Diagnostics.hpp"

namespace zcc {

// 符号类型
enum class SymbolKind {
    Variable,
    Constant,
    Function,
    Class,
    Module,
    Parameter,
    Field,
    Method,
    Import,
    Unknown
};

// 符号信息
struct Symbol {
    std::string name;
    SymbolKind kind;
    SourceLocation location;
    int scopeDepth;
    int slot;           // 栈槽位置
    bool isMutable;     // 是否可变
    bool isExported;    // 是否导出
    std::string type;   // 类型信息 (预留)
    
    Symbol(std::string n, SymbolKind k, const SourceLocation& loc, 
           int depth = 0, int slot = -1)
        : name(std::move(n)), kind(k), location(loc),
          scopeDepth(depth), slot(slot), isMutable(true), isExported(false) {}
    
    std::string kindString() const {
        switch (kind) {
            case SymbolKind::Variable: return "变量";
            case SymbolKind::Constant: return "常量";
            case SymbolKind::Function: return "函数";
            case SymbolKind::Class: return "类";
            case SymbolKind::Module: return "模块";
            case SymbolKind::Parameter: return "参数";
            case SymbolKind::Field: return "字段";
            case SymbolKind::Method: return "方法";
            case SymbolKind::Import: return "导入";
            default: return "未知";
        }
    }
};

// 作用域类
class Scope {
public:
    using SymbolPtr = std::shared_ptr<Symbol>;
    using SymbolMap = std::unordered_map<std::string, SymbolPtr>;
    
    Scope(Scope* parent = nullptr, int depth = 0)
        : parent_(parent), depth_(depth) {}
    
    // 定义符号
    SymbolPtr define(const std::string& name, SymbolKind kind, 
                     const SourceLocation& loc, bool isMutable = true) {
        if (symbols_.find(name) != symbols_.end()) {
            return nullptr; // 符号已存在
        }
        
        int slot = static_cast<int>(symbols_.size());
        auto symbol = std::make_shared<Symbol>(name, kind, loc, depth_, slot);
        symbol->isMutable = isMutable;
        symbols_[name] = symbol;
        return symbol;
    }
    
    // 查找符号 (从当前作用域向外)
    SymbolPtr lookup(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second;
        }
        if (parent_) {
            return parent_->lookup(name);
        }
        return nullptr;
    }
    
    // 仅在当前作用域查找
    SymbolPtr lookupLocal(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 获取所有符号
    const SymbolMap& symbols() const { return symbols_; }
    
    // 获取父作用域
    Scope* parent() const { return parent_; }
    
    // 获取作用域深度
    int depth() const { return depth_; }

private:
    Scope* parent_;
    int depth_;
    SymbolMap symbols_;
};

// 符号表类
class SymbolTable {
public:
    SymbolTable() : currentScope_(nullptr), scopeDepth_(0) {
        // 创建全局作用域
        globalScope_ = std::make_unique<Scope>(nullptr, 0);
        currentScope_ = globalScope_.get();
    }
    
    // 进入新作用域
    Scope* beginScope() {
        scopeDepth_++;
        auto newScope = std::make_unique<Scope>(currentScope_, scopeDepth_);
        Scope* scope = newScope.get();
        scopes_.push_back(std::move(newScope));
        currentScope_ = scope;
        return currentScope_;
    }
    
    // 退出作用域
    void endScope() {
        if (currentScope_ && currentScope_->parent()) {
            currentScope_ = currentScope_->parent();
            scopeDepth_--;
        }
    }
    
    // 定义符号
    Scope::SymbolPtr define(const std::string& name, SymbolKind kind,
                            const SourceLocation& loc, bool isMutable = true) {
        if (!currentScope_) return nullptr;
        return currentScope_->define(name, kind, loc, isMutable);
    }
    
    // 查找符号
    Scope::SymbolPtr lookup(const std::string& name) {
        if (!currentScope_) return nullptr;
        return currentScope_->lookup(name);
    }
    
    // 获取全局作用域
    Scope* globalScope() const { return globalScope_.get(); }
    
    // 获取当前作用域
    Scope* current() const { return currentScope_; }
    
    // 获取作用域深度
    int scopeDepth() const { return scopeDepth_; }
    
    // 获取所有作用域
    const std::vector<std::unique_ptr<Scope>>& scopes() const { return scopes_; }

private:
    std::unique_ptr<Scope> globalScope_;
    Scope* currentScope_;
    std::vector<std::unique_ptr<Scope>> scopes_;
    int scopeDepth_;
};

} // namespace zcc

#endif // ZCC_SYMBOLTABLE_HPP
