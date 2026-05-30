#ifndef ZCC_OPTIMIZER_HPP
#define ZCC_OPTIMIZER_HPP

#include "ZCCIR.hpp"
#include "Common.hpp"
#include <memory>
#include <functional>
#include <set>
#include <map>

namespace zcc {

// 使用 Common.hpp 中定义的 OptimizationLevel

// 优化 Pass 基类
class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    
    // Pass 名称
    virtual std::string getName() const = 0;
    
    // 运行优化
    // 返回是否修改了 IR
    virtual bool run(ZCCIRModule& module) = 0;
    
    // 是否应该在特定优化级别运行
    virtual bool shouldRunAt(OptimizationLevel level) const = 0;
};

// ==================== 优化器 ====================

class Optimizer {
public:
    Optimizer(OptimizationLevel level = OptimizationLevel::O0);
    
    // 注册优化 Pass
    void registerPass(std::unique_ptr<OptimizationPass> pass);
    
    // 运行所有优化
    bool optimize(ZCCIRModule& module);
    
    // 运行单个 Pass
    bool runPass(ZCCIRModule& module, const std::string& passName);
    
    // 获取优化统计
    struct Stats {
        size_t passesRun;
        size_t totalIterations;
        std::map<std::string, size_t> passStats;
    };
    
    const Stats& getStats() const { return stats_; }
    
private:
    OptimizationLevel level_;
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
    Stats stats_;
};

// ==================== 常量折叠 Pass ====================

// 常量折叠：在编译时计算常量表达式
class ConstantFoldingPass : public OptimizationPass {
public:
    std::string getName() const override { return "ConstantFolding"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O1;
    }
    
private:
    bool foldFunction(ZCCIRFunction& func);
    bool foldBlock(ZCCIRBlock& block);
    bool foldInstruction(ZCCIRInstruction& instr);
};

// ==================== 死代码消除 Pass ====================

// 死代码消除：删除未使用的指令和变量
class DeadCodeEliminationPass : public OptimizationPass {
public:
    std::string getName() const override { return "DeadCodeElimination"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O1;
    }
    
private:
    // 计算活跃变量
    void computeLiveness(ZCCIRFunction& func);
    
    // 删除死代码
    bool eliminateDeadCode(ZCCIRFunction& func);
    
    std::map<ZCCIRBlock*, std::set<ZCCIRValue::ID>> liveIn_;
    std::map<ZCCIRBlock*, std::set<ZCCIRValue::ID>> liveOut_;
};

// ==================== 公共子表达式消除 Pass ====================

// CSE：消除重复计算
class CommonSubexpressionEliminationPass : public OptimizationPass {
public:
    std::string getName() const override { return "CSE"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O2;
    }
    
private:
    bool eliminateCSE(ZCCIRFunction& func);
    
    // 表达式哈希
    struct ExpressionHash {
        size_t operator()(const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& expr) const;
    };
    
    // 表达式相等
    struct ExpressionEqual {
        bool operator()(
            const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& a,
            const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& b
        ) const;
    };
    
    std::unordered_map<
        std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>,
        ZCCIRValue::ID,
        ExpressionHash,
        ExpressionEqual
    > availableExpressions_;
};

// ==================== 内联 Pass ====================

// 函数内联：将小函数内联到调用处
class InliningPass : public OptimizationPass {
public:
    std::string getName() const override { return "Inlining"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O2;
    }
    
    // 设置内联阈值（指令数）
    void setThreshold(size_t threshold) { threshold_ = threshold; }
    
private:
    // 是否应该内联
    bool shouldInline(const ZCCIRFunction& func);
    
    // 内联函数
    bool inlineFunction(ZCCIRModule& module, ZCCIRFunction& caller, 
                       ZCCIRInstruction& callInstr, const ZCCIRFunction& callee);
    
    size_t threshold_ = 10;  // 默认内联阈值
};

// ==================== 循环优化 Pass ====================

// 循环不变量外提
class LoopInvariantCodeMotionPass : public OptimizationPass {
public:
    std::string getName() const override { return "LICM"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O2;
    }
    
private:
    // 查找循环
    struct Loop {
        ZCCIRBlock* header;
        ZCCIRBlock* latch;
        std::set<ZCCIRBlock*> blocks;
    };
    
    std::vector<Loop> findLoops(ZCCIRFunction& func);
    bool moveInvariantCode(Loop& loop);
};

// 循环展开
class LoopUnrollPass : public OptimizationPass {
public:
    std::string getName() const override { return "LoopUnroll"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O3;
    }
    
    // 设置展开因子
    void setUnrollFactor(size_t factor) { unrollFactor_ = factor; }
    
private:
    bool unrollLoop(ZCCIRFunction& func, ZCCIRBlock* header, ZCCIRBlock* latch);
    
    size_t unrollFactor_ = 4;
};

// ==================== 寄存器分配准备 Pass ====================

// SSA 破坏：将 SSA 形式转换为可执行代码
class SSADestructionPass : public OptimizationPass {
public:
    std::string getName() const override { return "SSADestruction"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O0;  // 总是运行
    }
    
private:
    // 处理 Phi 节点
    bool eliminatePhiNodes(ZCCIRFunction& func);
    
    // 插入并行拷贝
    void insertParallelCopies(ZCCIRBlock& block,
                             const std::vector<std::pair<ZCCIRValue::ID, ZCCIRValue::ID>>& copies);
};

// ==================== 窥孔优化 Pass ====================

// 窥孔优化：小范围内的局部优化
class PeepholeOptimizationPass : public OptimizationPass {
public:
    std::string getName() const override { return "Peephole"; }
    
    bool run(ZCCIRModule& module) override;
    
    bool shouldRunAt(OptimizationLevel level) const override {
        return level >= OptimizationLevel::O1;
    }
    
private:
    bool optimizeBlock(ZCCIRBlock& block);
    
    // 优化模式
    bool optimizeAddZero(ZCCIRInstruction& instr);      // x + 0 -> x
    bool optimizeMulOne(ZCCIRInstruction& instr);       // x * 1 -> x
    bool optimizeMulZero(ZCCIRInstruction& instr);      // x * 0 -> 0
    bool optimizeSubSelf(ZCCIRInstruction& instr);      // x - x -> 0
    bool optimizeDivOne(ZCCIRInstruction& instr);       // x / 1 -> x
    bool optimizeDoubleNot(ZCCIRInstruction& instr);    // !!x -> x
    bool optimizeConstFold(ZCCIRInstruction& instr);    // 常量折叠
};

// ==================== 优化器工厂 ====================

// 创建标准优化流水线
inline std::unique_ptr<Optimizer> createOptimizer(OptimizationLevel level) {
    auto optimizer = std::make_unique<Optimizer>(level);
    
    if (level >= OptimizationLevel::O0) {
        optimizer->registerPass(std::make_unique<SSADestructionPass>());
    }
    
    if (level >= OptimizationLevel::O1) {
        optimizer->registerPass(std::make_unique<ConstantFoldingPass>());
        optimizer->registerPass(std::make_unique<DeadCodeEliminationPass>());
        optimizer->registerPass(std::make_unique<PeepholeOptimizationPass>());
    }
    
    if (level >= OptimizationLevel::O2) {
        optimizer->registerPass(std::make_unique<CommonSubexpressionEliminationPass>());
        optimizer->registerPass(std::make_unique<InliningPass>());
        optimizer->registerPass(std::make_unique<LoopInvariantCodeMotionPass>());
    }
    
    if (level >= OptimizationLevel::O3) {
        optimizer->registerPass(std::make_unique<LoopUnrollPass>());
    }
    
    return optimizer;
}

} // namespace zcc

#endif // ZCC_OPTIMIZER_HPP
