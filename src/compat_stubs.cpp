// v0.2 桩文件 - 提供头文件中声明但未实现的方法
#include "ZCCIR.hpp"
#include "Types.hpp"
#include "Platform.hpp"
#include <stdexcept>
#include <utility>

namespace zcc {

// ==================== TypeManager ====================
TypeManager& TypeManager::getInstance() {
    static TypeManager instance;
    return instance;
}

ZCCTypePtr TypeManager::getVoid() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::VOID); }
ZCCTypePtr TypeManager::getBool() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::BOOL); }
ZCCTypePtr TypeManager::getI32() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::I32); }
ZCCTypePtr TypeManager::getI64() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::I64); }
ZCCTypePtr TypeManager::getF64() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::F64); }
ZCCTypePtr TypeManager::getString() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::STRING); }
ZCCTypePtr TypeManager::getAny() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::ANY); }
ZCCTypePtr TypeManager::getBigint() { return std::make_shared<PrimitiveTypeImpl>(PrimitiveType::BIGINT); }

// ==================== FunctionType ====================
std::string FunctionType::buildName(const ZCCTypePtr& returnType,
                                     const ParamList& params,
                                     bool varargs) {
    std::string n = "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) n += ", ";
        n += params[i].second->getName();
    }
    if (varargs) n += ", ...";
    n += ") -> " + returnType->getName();
    return n;
}

bool FunctionType::equals(const ZCCType& other) const {
    return false;
}

std::string FunctionType::toString() const {
    return getName();
}

// ==================== PrimitiveTypeImpl ====================
std::string PrimitiveTypeImpl::getPrimitiveName(PrimitiveType pt) {
    switch (pt) {
        case PrimitiveType::VOID: return "void";
        case PrimitiveType::BOOL: return "bool";
        case PrimitiveType::I8: return "i8";
        case PrimitiveType::I16: return "i16";
        case PrimitiveType::I32: return "i32";
        case PrimitiveType::I64: return "i64";
        case PrimitiveType::U8: return "u8";
        case PrimitiveType::U16: return "u16";
        case PrimitiveType::U32: return "u32";
        case PrimitiveType::U64: return "u64";
        case PrimitiveType::F32: return "f32";
        case PrimitiveType::F64: return "f64";
        case PrimitiveType::BIGINT: return "bigint";
        case PrimitiveType::BIGFLOAT: return "bigfloat";
        case PrimitiveType::STRING: return "string";
        case PrimitiveType::ANY: return "any";
        default: return "unknown";
    }
}

bool PrimitiveTypeImpl::equals(const ZCCType& other) const {
    return getName() == other.getName();
}

std::string PrimitiveTypeImpl::toString() const {
    return getName();
}

size_t PrimitiveTypeImpl::getSize() const {
    switch (primitiveType_) {
        case PrimitiveType::VOID: return 0;
        case PrimitiveType::BOOL: case PrimitiveType::I8: case PrimitiveType::U8: return 1;
        case PrimitiveType::I16: case PrimitiveType::U16: return 2;
        case PrimitiveType::I32: case PrimitiveType::U32: case PrimitiveType::F32: return 4;
        case PrimitiveType::I64: case PrimitiveType::U64: case PrimitiveType::F64: return 8;
        default: return 8;
    }
}

size_t PrimitiveTypeImpl::getAlignment() const { return getSize(); }

// ==================== ZCCType ====================
bool ZCCType::isCompatibleWith(const ZCCType& other) const {
    return getName() == other.getName();
}

bool ZCCType::isSubtypeOf(const ZCCType& other) const {
    return getName() == other.getName();
}

// ==================== ZCCIRBlock ====================
void ZCCIRBlock::addSuccessor(ZCCIRBlock* block) {
    if (block) successors_.push_back(block);
}

void ZCCIRBlock::addPredecessor(ZCCIRBlock* block) {
    if (block) predecessors_.push_back(block);
}

bool ZCCIRBlock::isTerminated() const {
    if (instructions_.empty()) return false;
    auto op = instructions_.back()->getOpcode();
    return op == ZCCIROpcode::RET || op == ZCCIROpcode::JMP || 
           op == ZCCIROpcode::JMP_IF || op == ZCCIROpcode::JMP_IF_NOT ||
           op == ZCCIROpcode::THROW || op == ZCCIROpcode::UNREACHABLE;
}

// ==================== ZCCIRFunction ====================
ZCCIRBlock* ZCCIRFunction::createBlock(const std::string& label) {
    auto block = std::make_unique<ZCCIRBlock>(label);
    auto* ptr = block.get();
    blocks_.push_back(std::move(block));
    return ptr;
}

// ==================== ZCCIRModule ====================
ZCCIRFunction* ZCCIRModule::createFunction(
    const std::string& name,
    ZCCTypePtr returnType,
    std::vector<std::pair<std::string, ZCCTypePtr>> args) {
    auto func = std::make_unique<ZCCIRFunction>(name, std::move(returnType), std::move(args));
    auto* ptr = func.get();
    functions_.push_back(std::move(func));
    return ptr;
}

std::shared_ptr<ZCCIRConstant> ZCCIRModule::createConstant(
    ZCCIRConstant::ConstantValue value, ZCCTypePtr type) {
    auto c = std::make_shared<ZCCIRConstant>(std::move(value), std::move(type), nextValueID_++);
    constants_.push_back(c);
    return c;
}

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

// ==================== ZCCIRBlock (addInstruction) ====================
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

TargetPlatform TargetPlatform::parse(const std::string& /*triple*/) {
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
