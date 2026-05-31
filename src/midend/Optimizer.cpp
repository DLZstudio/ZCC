#include "Optimizer.hpp"
#include <algorithm>
#include <cstring>

namespace zcc {

// ==================== 优化器实现 ====================

Optimizer::Optimizer(OptimizationLevel level) : level_(level) {}

void Optimizer::registerPass(std::unique_ptr<OptimizationPass> pass) {
    if (pass && pass->shouldRunAt(level_)) {
        passes_.push_back(std::move(pass));
    }
}

bool Optimizer::optimize(ZCCIRModule& module) {
    bool modified = false;
    
    for (const auto& pass : passes_) {
        stats_.passesRun++;
        
        bool passModified;
        do {
            stats_.totalIterations++;
            passModified = pass->run(module);
            if (passModified) {
                modified = true;
                stats_.passStats[pass->getName()]++;
            }
        } while (passModified);  // 迭代直到不动点
    }
    
    return modified;
}

bool Optimizer::runPass(ZCCIRModule& module, const std::string& passName) {
    for (auto& pass : passes_) {
        if (pass->getName() == passName) {
            return pass->run(module);
        }
    }
    return false;
}

// ==================== 常量折叠实现 ====================

bool ConstantFoldingPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    for (auto& func : module.getFunctions()) {
        if (!func->isExternal()) {
            modified |= foldFunction(*func);
        }
    }
    
    return modified;
}

bool ConstantFoldingPass::foldFunction(ZCCIRFunction& func) {
    bool modified = false;
    
    for (auto& block : func.getBlocks()) {
        modified |= foldBlock(*block);
    }
    
    return modified;
}

bool ConstantFoldingPass::foldBlock(ZCCIRBlock& block) {
    bool modified = false;
    
    for (auto& instr : block.getInstructions()) {
        modified |= foldInstruction(*instr);
    }
    
    return modified;
}

bool ConstantFoldingPass::foldInstruction(ZCCIRInstruction& instr) {
    // TODO: 实现常量折叠逻辑
    // 检查操作数是否都是常量
    // 如果是，计算结果并替换指令
    
    switch (instr.getOpcode()) {
        case ZCCIROpcode::ADD:
        case ZCCIROpcode::SUB:
        case ZCCIROpcode::MUL:
        case ZCCIROpcode::DIV:
            // 检查操作数并折叠
            break;
            
        default:
            break;
    }
    
    return false;
}

// ==================== 死代码消除实现 ====================

bool DeadCodeEliminationPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    for (auto& func : module.getFunctions()) {
        if (!func->isExternal()) {
            computeLiveness(*func);
            modified |= eliminateDeadCode(*func);
        }
    }
    
    return modified;
}

void DeadCodeEliminationPass::computeLiveness(ZCCIRFunction& func) {
    // TODO: 实现活跃变量分析
    // 数据流分析：从后向前传播活跃信息
}

bool DeadCodeEliminationPass::eliminateDeadCode(ZCCIRFunction& func) {
    bool modified = false;
    
    // TODO: 删除未使用的指令
    // 保留：有副作用的指令、结果被使用的指令
    
    return modified;
}

// ==================== 窥孔优化实现 ====================

bool PeepholeOptimizationPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    for (auto& func : module.getFunctions()) {
        for (auto& block : func->getBlocks()) {
            modified |= optimizeBlock(*block);
        }
    }
    
    return modified;
}

bool PeepholeOptimizationPass::optimizeBlock(ZCCIRBlock& block) {
    bool modified = false;
    
    auto& instructions = block.getInstructions();
    auto it = instructions.begin();
    
    while (it != instructions.end()) {
        auto& instr = **it;
        bool changed = false;
        
        changed |= optimizeAddZero(instr);
        changed |= optimizeMulOne(instr);
        changed |= optimizeMulZero(instr);
        changed |= optimizeSubSelf(instr);
        changed |= optimizeDivOne(instr);
        changed |= optimizeDoubleNot(instr);
        changed |= optimizeConstFold(instr);
        
        if (changed) {
            modified = true;
        } else {
            ++it;
        }
    }
    
    return modified;
}

bool PeepholeOptimizationPass::optimizeAddZero(ZCCIRInstruction& instr) {
    // x + 0 -> x
    if (instr.getOpcode() != ZCCIROpcode::ADD) return false;
    
    // TODO: 检查第二个操作数是否为 0
    // 如果是，将结果重定向到第一个操作数
    
    return false;
}

bool PeepholeOptimizationPass::optimizeMulOne(ZCCIRInstruction& instr) {
    // x * 1 -> x
    if (instr.getOpcode() != ZCCIROpcode::MUL) return false;
    
    // TODO: 检查第二个操作数是否为 1
    
    return false;
}

bool PeepholeOptimizationPass::optimizeMulZero(ZCCIRInstruction& instr) {
    // x * 0 -> 0
    if (instr.getOpcode() != ZCCIROpcode::MUL) return false;
    
    // TODO: 检查第二个操作数是否为 0
    
    return false;
}

bool PeepholeOptimizationPass::optimizeSubSelf(ZCCIRInstruction& instr) {
    // x - x -> 0
    if (instr.getOpcode() != ZCCIROpcode::SUB) return false;
    
    // TODO: 检查两个操作数是否相同
    
    return false;
}

bool PeepholeOptimizationPass::optimizeDivOne(ZCCIRInstruction& instr) {
    // x / 1 -> x
    if (instr.getOpcode() != ZCCIROpcode::DIV) return false;
    
    // TODO: 检查第二个操作数是否为 1
    
    return false;
}

bool PeepholeOptimizationPass::optimizeDoubleNot(ZCCIRInstruction& instr) {
    // !!x -> x
    if (instr.getOpcode() != ZCCIROpcode::NOT) return false;
    
    // TODO: 检查操作数是否是另一个 NOT 指令的结果
    
    return false;
}

bool PeepholeOptimizationPass::optimizeConstFold(ZCCIRInstruction& instr) {
    // 常量折叠
    // TODO: 如果所有操作数都是常量，计算结果
    
    return false;
}

// ==================== CSE 实现 ====================

bool CommonSubexpressionEliminationPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    for (auto& func : module.getFunctions()) {
        modified |= eliminateCSE(*func);
    }
    
    return modified;
}

bool CommonSubexpressionEliminationPass::eliminateCSE(ZCCIRFunction& func) {
    availableExpressions_.clear();
    bool modified = false;
    
    // TODO: 实现 CSE 算法
    // 1. 遍历基本块
    // 2. 维护可用表达式表
    // 3. 替换重复计算
    
    return modified;
}

size_t CommonSubexpressionEliminationPass::ExpressionHash::operator()(
    const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& expr) const {
    
    size_t hash = static_cast<size_t>(expr.first);
    for (auto id : expr.second) {
        hash ^= id + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

bool CommonSubexpressionEliminationPass::ExpressionEqual::operator()(
    const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& a,
    const std::pair<ZCCIROpcode, std::vector<ZCCIRValue::ID>>& b) const {
    
    if (a.first != b.first) return false;
    if (a.second.size() != b.second.size()) return false;
    
    for (size_t i = 0; i < a.second.size(); ++i) {
        if (a.second[i] != b.second[i]) return false;
    }
    
    return true;
}

// ==================== 内联实现 ====================

bool InliningPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    // 收集可内联的函数
    std::vector<std::string> inlineable;
    for (const auto& func : module.getFunctions()) {
        if (!func->isExternal() && shouldInline(*func)) {
            inlineable.push_back(func->getName());
        }
    }
    
    // 内联调用
    for (auto& func : module.getFunctions()) {
        if (!func->isExternal()) {
            for (auto& block : func->getBlocks()) {
                for (auto& instr : block->getInstructions()) {
                    if (instr->getOpcode() == ZCCIROpcode::CALL) {
                        // TODO: 检查是否调用可内联函数并内联
                    }
                }
            }
        }
    }
    
    return modified;
}

bool InliningPass::shouldInline(const ZCCIRFunction& func) {
    // 计算函数大小（指令数）
    size_t instructionCount = 0;
    for (const auto& block : func.getBlocks()) {
        instructionCount += block->getInstructions().size();
    }
    
    return instructionCount <= threshold_;
}

// ==================== SSA 破坏实现 ====================

bool SSADestructionPass::run(ZCCIRModule& module) {
    bool modified = false;
    
    for (auto& func : module.getFunctions()) {
        if (!func->isExternal()) {
            modified |= eliminatePhiNodes(*func);
        }
    }
    
    return modified;
}

bool SSADestructionPass::eliminatePhiNodes(ZCCIRFunction& func) {
    bool modified = false;
    
    // TODO: 消除 Phi 节点
    // 1. 对于每个 Phi 节点，在 predecessors 末尾插入拷贝
    // 2. 替换 Phi 节点的使用
    
    return modified;
}

} // namespace zcc
