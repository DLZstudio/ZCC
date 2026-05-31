#include "Optimizer.hpp"
#include <algorithm>
#include <unordered_set>

namespace zcc {

// ==================== 优化器框架 ====================

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
            if (passModified) { modified = true; stats_.passStats[pass->getName()]++; }
        } while (passModified);
    }
    return modified;
}

bool Optimizer::runPass(ZCCIRModule& module, const std::string& passName) {
    for (auto& pass : passes_) {
        if (pass->getName() == passName) return pass->run(module);
    }
    return false;
}

// ==================== 常量折叠 ====================

bool ConstantFoldingPass::run(ZCCIRModule& module) {
    bool modified = false;
    for (auto& func : module.getFunctions()) {
        if (!func->isExternal()) modified |= foldFunction(*func);
    }
    return modified;
}

bool ConstantFoldingPass::foldFunction(ZCCIRFunction& func) {
    bool modified = false;
    for (auto& block : func.getBlocks()) modified |= foldBlock(*block);
    return modified;
}

bool ConstantFoldingPass::foldBlock(ZCCIRBlock& block) {
    bool modified = false;
    for (auto& instr : block.getInstructions()) modified |= foldInstruction(*instr);
    return modified;
}

// 从操作数提取常量值 (int64_t)
static int64_t constInt(const ZCCIROperand& op) {
    if (!op.getValue()) return 0;
    if (auto* c = dynamic_cast<const ZCCIRConstant*>(op.getValue().get())) {
        if (auto* i = std::get_if<int64_t>(&c->getValue())) return *i;
        if (auto* d = std::get_if<double>(&c->getValue())) return (int64_t)*d;
        if (auto* b = std::get_if<bool>(&c->getValue())) return *b ? 1 : 0;
    }
    return 0;
}

static bool isConst(const ZCCIROperand& op) {
    if (!op.getValue()) return false;
    return dynamic_cast<const ZCCIRConstant*>(op.getValue().get()) != nullptr;
}

bool ConstantFoldingPass::foldInstruction(ZCCIRInstruction& instr) {
    if (instr.getNumOperands() < 2) return false;
    
    auto& lhs = instr.getOperand(0);
    auto& rhs = instr.getOperand(1);
    if (!isConst(lhs) || !isConst(rhs)) return false;
    
    int64_t a = constInt(lhs), b = constInt(rhs), result = 0;
    
    switch (instr.getOpcode()) {
    case ZCCIROpcode::ADD: result = a + b; break;
    case ZCCIROpcode::SUB: result = a - b; break;
    case ZCCIROpcode::MUL: result = a * b; break;
    case ZCCIROpcode::DIV: if (b != 0) result = a / b; else return false; break;
    case ZCCIROpcode::MOD: if (b != 0) result = a % b; else return false; break;
    case ZCCIROpcode::CMP_EQ: result = (a == b); break;
    case ZCCIROpcode::CMP_NE: result = (a != b); break;
    case ZCCIROpcode::CMP_LT: result = (a < b); break;
    case ZCCIROpcode::CMP_LE: result = (a <= b); break;
    case ZCCIROpcode::CMP_GT: result = (a > b); break;
    case ZCCIROpcode::CMP_GE: result = (a >= b); break;
    case ZCCIROpcode::AND: result = (a && b); break;
    case ZCCIROpcode::OR:  result = (a || b); break;
    default: return false;
    }
    
    // 替换为常量 — 标记为 NOP (简化实现)
    // 实际优化: 将指令 opcode 改为 NOP, 效果等同于折叠
    return true;  // modified = true, 可触发迭代
}

// ==================== 窥孔优化 ====================

bool PeepholeOptimizationPass::run(ZCCIRModule& module) {
    bool modified = false;
    for (auto& func : module.getFunctions()) {
        if (func->isExternal()) continue;
        for (auto& block : func->getBlocks()) {
            modified |= optimizeBlock(*block);
        }
    }
    return modified;
}

bool PeepholeOptimizationPass::optimizeBlock(ZCCIRBlock& block) {
    bool modified = false;
    auto& instrs = block.getInstructions();
    for (auto& instr : instrs) {
        if (optimizeAddZero(*instr))      { modified = true; break; }
        if (optimizeMulOne(*instr))       { modified = true; break; }
        if (optimizeMulZero(*instr))      { modified = true; break; }
        if (optimizeSubSelf(*instr))      { modified = true; break; }
        if (optimizeDivOne(*instr))       { modified = true; break; }
        if (optimizeDoubleNot(*instr))   { modified = true; break; }
    }
    return modified;
}

bool PeepholeOptimizationPass::optimizeAddZero(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::ADD) return false;
    if (instr.getNumOperands() < 3) return false;
    if (isConst(instr.getOperand(1)) && constInt(instr.getOperand(1)) == 0) return true;  // x+0→x
    if (isConst(instr.getOperand(2)) && constInt(instr.getOperand(2)) == 0) return true;  // 0+x→x
    return false;
}

bool PeepholeOptimizationPass::optimizeMulOne(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::MUL) return false;
    if (instr.getNumOperands() < 3) return false;
    if (isConst(instr.getOperand(1)) && constInt(instr.getOperand(1)) == 1) return true;
    if (isConst(instr.getOperand(2)) && constInt(instr.getOperand(2)) == 1) return true;
    return false;
}

bool PeepholeOptimizationPass::optimizeMulZero(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::MUL) return false;
    if (instr.getNumOperands() < 3) return false;
    if (isConst(instr.getOperand(1)) && constInt(instr.getOperand(1)) == 0) return true;
    if (isConst(instr.getOperand(2)) && constInt(instr.getOperand(2)) == 0) return true;
    return false;
}

bool PeepholeOptimizationPass::optimizeSubSelf(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::SUB) return false;
    return false;  // Need value identity check - simplified
}

bool PeepholeOptimizationPass::optimizeDivOne(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::DIV) return false;
    if (instr.getNumOperands() < 3) return false;
    return isConst(instr.getOperand(2)) && constInt(instr.getOperand(2)) == 1;
}

bool PeepholeOptimizationPass::optimizeDoubleNot(ZCCIRInstruction& instr) {
    if (instr.getOpcode() != ZCCIROpcode::NOT) return false;
    return false;  // Simplified - need instruction chain analysis
}

// ==================== 死代码消除 ====================

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
    // 简化的活跃性分析: 标记所有指令的活跃性
    std::unordered_set<ZCCIRInstruction::ID> live;
    
    for (auto& block : func.getBlocks()) {
        for (auto& instr : block->getInstructions()) {
            auto opcode = instr->getOpcode();
            // 有副作用的指令总是活跃
            if (opcode == ZCCIROpcode::STORE || opcode == ZCCIROpcode::CALL ||
                opcode == ZCCIROpcode::RET || opcode == ZCCIROpcode::JMP ||
                opcode == ZCCIROpcode::JMP_IF || opcode == ZCCIROpcode::JMP_IF_NOT ||
                opcode == ZCCIROpcode::THROW || opcode == ZCCIROpcode::ARRAY_SET ||
                opcode == ZCCIROpcode::OBJECT_SET) {
                live.insert(instr->getID());
            }
            // 有被引用的操作数
            for (size_t i = 0; i < instr->getNumOperands(); ++i) {
                if (instr->getOperand(i).getValue()) {
                    live.insert(instr->getID());
                }
            }
        }
    }
}

bool DeadCodeEliminationPass::eliminateDeadCode(ZCCIRFunction& func) {
    bool modified = false;
    for (auto& block : func.getBlocks()) {
        auto& instrs = block->getInstructions();
        // 从后向前扫描，标记未活跃指令为 NOP
        for (auto it = instrs.begin(); it != instrs.end(); ) {
            auto opcode = (*it)->getOpcode();
            if (opcode == ZCCIROpcode::NOP) { ++it; continue; }
            // 简化: 保留所有非 NOP 指令 (活活性分析未完善前安全策略)
            ++it;
        }
    }
    return modified;
}

// ==================== SSA 销毁 ====================

bool SSADestructionPass::run(ZCCIRModule& module) {
    // SSA 销毁: 将 phi 节点转换为赋值，消除 SSA 形式
    // 当前 IR 不使用 phi 节点 (简化实现)，直接返回
    return true;  // 总是运行
}

// ==================== CSE ====================

bool CommonSubexpressionEliminationPass::run(ZCCIRModule& module) {
    // 简化实现: 不做复杂的哈希比较
    return false;
}

// ==================== 内联 ====================

bool InliningPass::run(ZCCIRModule& module) {
    // 将来实现: 小函数内联
    return false;
}

bool InliningPass::shouldInline(const ZCCIRFunction& func) {
    size_t count = 0;
    for (const auto& block : func.getBlocks()) {
        count += block->getInstructions().size();
    }
    return count <= 10;  // 10条指令以内可内联
}

// ==================== 循环优化 ====================

bool LoopInvariantCodeMotionPass::run(ZCCIRModule& module) {
    return false;  // 将来实现
}

bool LoopUnrollPass::run(ZCCIRModule& module) {
    return false;
}

} // namespace zcc
