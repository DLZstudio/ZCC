// v0.2 桩文件 - 提供头文件中声明但未实现的方法
#include "ZCCIR.hpp"
#include "Platform.hpp"
#include <stdexcept>

namespace zcc {

// ==================== ZCCIRValue ====================
std::string ZCCIRValue::getName() const {
    return name_.empty() ? "%" + std::to_string(getID()) : name_;
}

void ZCCIRValue::setName(const std::string& name) {
    name_ = name;
}

// ==================== ZCCIRConstant ====================
std::string ZCCIRConstant::toString() const {
    return "<const>";
}

// ==================== ZCCIRInstruction ====================
void ZCCIRInstruction::setOperand(size_t idx, ZCCIROperand operand) {
    if (idx < operands_.size()) {
        operands_[idx] = std::move(operand);
    }
}

void ZCCIRInstruction::addOperand(ZCCIROperand operand) {
    operands_.push_back(std::move(operand));
}

std::string ZCCIRInstruction::toString() const {
    return "<instr>";
}

// ==================== ZCCIRBlock ====================
ZCCIRInstruction* ZCCIRBlock::addInstruction(std::unique_ptr<ZCCIRInstruction> instr) {
    auto* ptr = instr.get();
    instructions_.push_back(std::move(instr));
    return ptr;
}

// ==================== TargetPlatform ====================
std::string TargetPlatform::getArchString() const { return "x86_64"; }
std::string TargetPlatform::getVendorString() const { return "pc"; }
std::string TargetPlatform::getOSString() const { return "windows"; }
std::string TargetPlatform::getABIString() const { return "default"; }

TargetPlatform TargetPlatform::getCurrent() {
    return TargetPlatform{TargetArch::X86_64, TargetVendor::PC, TargetOS::WINDOWS, TargetABI::DEFAULT};
}

TargetPlatform TargetPlatform::parse(const std::string& triple) {
    // 简单解析: arch-vendor-os-abi
    return getCurrent();
}

// ==================== PlatformManager ====================
PlatformManager& PlatformManager::getInstance() {
    static PlatformManager instance;
    return instance;
}

void PlatformManager::initialize(TargetPlatform target) {
    currentPlatform_ = target;
}

} // namespace zcc
