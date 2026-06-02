#include "NativeBackend.hpp"
#include "Compiler.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <vector>
#include <new>

namespace zcc {

// === BigNum 字符串高精度算术 ===
// 内部用字符串实现，与 zcc_runtime.c 算法一致

static int bignumCmp(const std::string& a, const std::string& b);  // 前向声明

static std::string bignorm(const std::string& s) {
    // 去掉前导零和符号
    size_t i = 0; bool neg = false;
    if (i < s.size() && s[i] == '-') { neg = true; i++; }
    else if (i < s.size() && s[i] == '+') i++;
    while (i < s.size() - 1 && s[i] == '0') i++;
    std::string r = s.substr(i);
    if (neg && r != "0") r = "-" + r;
    return r.empty() ? "0" : r;
}

static std::string bignumAdd(const std::string& a, const std::string& b) {
    bool na = (!a.empty() && a[0] == '-');
    bool nb = (!b.empty() && b[0] == '-');
    std::string sa = na ? a.substr(1) : a;
    std::string sb = nb ? b.substr(1) : b;
    if (sa.empty()) sa = "0";
    if (sb.empty()) sb = "0";
    
    if (na == nb) {
        // 同号：绝对值相加
        int carry = 0;
        std::string r;
        int i = (int)sa.size() - 1, j = (int)sb.size() - 1;
        while (i >= 0 || j >= 0 || carry) {
            int da = (i >= 0) ? (sa[i--] - '0') : 0;
            int db = (j >= 0) ? (sb[j--] - '0') : 0;
            int s = da + db + carry;
            r += (char)('0' + s % 10);
            carry = s / 10;
        }
        std::reverse(r.begin(), r.end());
        if (na) r = "-" + r;
        return bignorm(r);
    } else {
        // 异号：绝对值相减
        bool swapNeg = false;
        if (sa.size() < sb.size() || (sa.size() == sb.size() && sa < sb)) {
            std::swap(sa, sb);
            swapNeg = true;
        }
        int borrow = 0;
        std::string r;
        int i = (int)sa.size() - 1, j = (int)sb.size() - 1;
        while (i >= 0) {
            int da = (sa[i--] - '0') - borrow;
            int db = (j >= 0) ? (sb[j--] - '0') : 0;
            if (da < db) { da += 10; borrow = 1; }
            else borrow = 0;
            r += (char)('0' + da - db);
        }
        std::reverse(r.begin(), r.end());
        bool resultNeg = (swapNeg ? nb : na);
        if (resultNeg) r = "-" + r;
        return bignorm(r);
    }
}

static std::string bignumSub(const std::string& a, const std::string& b) {
    std::string nb = b;
    if (nb.empty() || nb == "0") return a;
    if (nb[0] == '-') nb = nb.substr(1);
    else nb = "-" + nb;
    return bignumAdd(a, nb);
}

static std::string bignumMul(const std::string& a, const std::string& b) {
    bool na = (!a.empty() && a[0] == '-');
    bool nb = (!b.empty() && b[0] == '-');
    std::string sa = na ? a.substr(1) : a;
    std::string sb = nb ? b.substr(1) : b;
    if (sa.empty() || sb.empty()) return "0";
    sa = bignorm(sa); sb = bignorm(sb);
    
    // 10^n 特化: 直接拼接零
    auto isPow10 = [](const std::string& s) -> int {
        if (s.size() < 2 || s[0] != '1') return -1;
        for (size_t i = 1; i < s.size(); i++) if (s[i] != '0') return -1;
        return (int)(s.size() - 1);
    };
    int pa = isPow10(sa), pb = isPow10(sb);
    if (pa >= 0 && pb >= 0) {
        std::string r = "1" + std::string(pa + pb, '0');
        if (na != nb) r = "-" + r;
        return r;
    }
    
    // Karatsuba 阈值: < 80 位用小学乘法
    const int KARA_THRESHOLD = 80;
    if ((int)sa.size() <= KARA_THRESHOLD || (int)sb.size() <= KARA_THRESHOLD) {
        int n = (int)sa.size(), m = (int)sb.size();
        std::vector<int> prod(n + m, 0);
        for (int i = n - 1; i >= 0; i--) {
            int da = sa[i] - '0';
            for (int j = m - 1; j >= 0; j--) {
                int p = da * (sb[j] - '0') + prod[i + j + 1];
                prod[i + j + 1] = p % 10;
                prod[i + j] += p / 10;
            }
        }
        std::string r;
        for (int v : prod) {
            if (!r.empty() || v != 0) r += (char)('0' + v);
        }
        if (r.empty()) r = "0";
        if (na != nb) r = "-" + r;
        return r;
    }
    
    // ===== Karatsuba 乘法 =====
    // 补零使长度一致
    int n = (int)std::max(sa.size(), sb.size());
    while ((int)sa.size() < n) sa = "0" + sa;
    while ((int)sb.size() < n) sb = "0" + sb;
    
    int m = n / 2;
    std::string a1 = sa.substr(0, n - m);  // 高位
    std::string a0 = sa.substr(n - m);     // 低位
    std::string b1 = sb.substr(0, n - m);
    std::string b0 = sb.substr(n - m);
    
    // 去前导零
    a1 = bignorm(a1); a0 = bignorm(a0);
    b1 = bignorm(b1); b0 = bignorm(b0);
    
    std::string z2 = bignumMul(a1, b1);                    // a1 * b1
    std::string z0 = bignumMul(a0, b0);                    // a0 * b0
    std::string a1a0 = bignumAdd(a1, a0);
    std::string b1b0 = bignumAdd(b1, b0);
    std::string z1 = bignumSub(bignumSub(bignumMul(a1a0, b1b0), z2), z0);  // (a1+a0)(b1+b0) - z2 - z0
    
    // z = z2 * 10^(2m) + z1 * 10^m + z0
    std::string r = z2 + std::string(2 * m, '0');
    r = bignumAdd(r, z1 + std::string(m, '0'));
    r = bignumAdd(r, z0);
    r = bignorm(r);
    if (na != nb) r = "-" + r;
    return r;
}

static std::string bignumDiv(const std::string& a, const std::string& b) {
    // a / b, 截断向零
    bool na = (!a.empty() && a[0] == '-');
    bool nb = (!b.empty() && b[0] == '-');
    std::string sa = na ? a.substr(1) : a, sb = nb ? b.substr(1) : b;
    if (sb == "0" || sb.empty()) return "0";
    sa = bignorm(sa); sb = bignorm(sb);
    
    if (sa.size() < sb.size() || (sa.size() == sb.size() && sa < sb)) return "0";
    
    // 10^n 特化: 10^pa / 10^pb = 10^(pa-pb) (pa >= pb)
    auto isPow10 = [](const std::string& s) -> int {
        if (s.size() < 2 || s[0] != '1') return -1;
        for (size_t i = 1; i < s.size(); i++) if (s[i] != '0') return -1;
        return (int)(s.size() - 1);
    };
    int pa = isPow10(sa), pb = isPow10(sb);
    if (pa >= 0 && pb >= 0 && pa >= pb) {
        std::string q = "1" + std::string(pa - pb, '0');
        if (na != nb) q = "-" + q;
        return q;
    }
    
    // 长除法 (逐位试除)
    std::string q, cur;
    for (char c : sa) {
        cur += c;
        cur = bignorm(cur);
        int count = 0;
        std::string subVal = sb;
        while (bignumCmp(cur, subVal) >= 0) {
            cur = bignumSub(cur, subVal);
            count++;
        }
        q += (char)('0' + count);
    }
    q = bignorm(q);
    if (na != nb && q != "0") q = "-" + q;
    return q;
}

static std::string bignumMod(const std::string& a, const std::string& b) {
    // a % b
    bool na = (!a.empty() && a[0] == '-');
    std::string sa = na ? a.substr(1) : a, sb;
    if (!b.empty() && b[0] == '-') sb = b.substr(1);
    else sb = b;
    if (sb == "0" || sb.empty()) return "0";
    
    std::string q = bignumDiv(sa, sb);
    std::string r = bignumSub(sa, bignumMul(q, sb));
    if (na && r != "0") r = bignumSub(sb, r);
    return bignorm(r);
}

static int bignumCmp(const std::string& a, const std::string& b) {
    bool na = (!a.empty() && a[0] == '-');
    bool nb = (!b.empty() && b[0] == '-');
    if (!na && nb) return 1;
    if (na && !nb) return -1;
    
    std::string sa = na ? a.substr(1) : a;
    std::string sb = nb ? b.substr(1) : b;
    sa = bignorm(sa); sb = bignorm(sb);
    
    int sign = na ? -1 : 1;
    if (sa.size() != sb.size())
        return (sa.size() > sb.size()) ? sign : -sign;
    if (sa > sb) return sign;
    if (sa < sb) return -sign;
    return 0;
}

static std::string bignumPow(const std::string& a, double exp) {
    // a^exp, BigNum 仅支持整数指数；负指数 → "0" (整数语义)
    int e = (int)exp;
    if (e < 0) return "0";
    if (e == 0) return "1";
    if (a == "0") return "0";
    if (a == "1") return "1";
    
    // 快速路径: 10^n → "1" + n个"0"
    std::string baseNorm = bignorm(a);
    if (baseNorm == "10") {
        return "1" + std::string((size_t)e, '0');
    }
    
    // 快速路径: (-10)^n
    if (baseNorm == "-10") {
        std::string r = "1" + std::string((size_t)e, '0');
        if (e % 2 == 1) r = "-" + r;
        return r;
    }
    
    // 通用: 二分快速幂 (O(log n) 次乘法)
    std::string r = "1";
    std::string base = a;
    while (e > 0) {
        if (e & 1) r = bignumMul(r, base);
        base = bignumMul(base, base);
        e >>= 1;
    }
    return bignorm(r);
}

// ==================== 解释器实现 ====================

RuntimeValue NativeBackend::Interpreter::execute() {
    if (!state_.getFunction("main")) {
        std::cerr << "Error: main function not found\n";
        return RuntimeValue();
    }
    
    const ZCCIRFunction* mainFunc = state_.getFunction("main");
    if (!mainFunc || mainFunc->getBlocks().empty()) {
        return RuntimeValue();
    }
    
    // 执行 main 函数 — 按 CFG 跳转，非线性遍历
    state_.pushScope();
    ZCCIRBlock* currentBlock = mainFunc->getEntryBlock();
    
    while (currentBlock && !state_.hasReturn()) {
        auto [nextBlock, result] = executeBlock(*currentBlock);
        if (state_.hasReturn()) break;
        currentBlock = nextBlock;
    }
    
    state_.popScope();
    return state_.getReturn();
}

NativeBackend::Interpreter::BlockResult NativeBackend::Interpreter::executeBlock(const ZCCIRBlock& block) {
    BlockResult br;
    
    for (const auto& instr : block.getInstructions()) {
        if (state_.hasReturn()) {
            br.value = state_.getReturn();
            br.nextBlock = nullptr;
            return br;
        }
        
        br.value = executeInstruction(*instr);
        
        // 指令结果 → 寄存器 (用于后续操作数引用)
        auto resultPtr = instr->getResult();
        if (resultPtr) {
            reg_[resultPtr->getID()] = br.value;
        }
        
        // 处理控制流终止指令
        switch (instr->getOpcode()) {
            case ZCCIROpcode::JMP: {
                auto successors = block.getSuccessors();
                br.nextBlock = successors.empty() ? nullptr : successors[0];
                return br;
            }
            case ZCCIROpcode::JMP_IF: {
                auto successors = block.getSuccessors();
                if (br.value.asBool() && successors.size() >= 1) {
                    br.nextBlock = successors[0];   // true 分支
                } else if (successors.size() >= 2) {
                    br.nextBlock = successors[1];   // false 分支
                }
                return br;
            }
            case ZCCIROpcode::JMP_IF_NOT: {
                auto successors = block.getSuccessors();
                if (!br.value.asBool() && successors.size() >= 1) {
                    br.nextBlock = successors[0];   // false 分支
                } else if (successors.size() >= 2) {
                    br.nextBlock = successors[1];   // true 分支
                }
                return br;
            }
            case ZCCIROpcode::RET:
                state_.returnValue(br.value);
                br.nextBlock = nullptr;
                return br;
            default:
                break;
        }
    }
    
    // 无终止指令 → 取第一个后继 (fallthrough)
    auto successors = block.getSuccessors();
    br.nextBlock = successors.empty() ? nullptr : successors[0];
    return br;
}

RuntimeValue NativeBackend::Interpreter::executeInstruction(const ZCCIRInstruction& instr) {
    auto op0 = [&]() { return resolveOperand(instr, 0); };
    auto op1 = [&]() { return resolveOperand(instr, 1); };
    auto op2 = [&]() { return resolveOperand(instr, 2); };
    
    switch (instr.getOpcode()) {
        case ZCCIROpcode::NOP:
            return RuntimeValue();
           
        // === 算术 === 
        case ZCCIROpcode::ADD:
            return RuntimeValue(op0().asNumber() + op1().asNumber());
        case ZCCIROpcode::SUB:
            return RuntimeValue(op0().asNumber() - op1().asNumber());
        case ZCCIROpcode::MUL:
            return RuntimeValue(op0().asNumber() * op1().asNumber());
        case ZCCIROpcode::DIV: {
            double d = op1().asNumber();
            return RuntimeValue(d != 0.0 ? op0().asNumber() / d : 0.0);
        }
        case ZCCIROpcode::MOD: {
            double d = op1().asNumber();
            return RuntimeValue(d != 0.0 ? fmod(op0().asNumber(), d) : 0.0);
        }
        case ZCCIROpcode::NEG:
            return RuntimeValue(-op0().asNumber());
        case ZCCIROpcode::POW:
            return RuntimeValue(pow(op0().asNumber(), op1().asNumber()));
            
        // === 比较 ===
        case ZCCIROpcode::CMP_EQ:
            return RuntimeValue(op0().asNumber() == op1().asNumber());
        case ZCCIROpcode::CMP_NE:
            return RuntimeValue(op0().asNumber() != op1().asNumber());
        case ZCCIROpcode::CMP_LT:
            return RuntimeValue(op0().asNumber() <  op1().asNumber());
        case ZCCIROpcode::CMP_LE:
            return RuntimeValue(op0().asNumber() <= op1().asNumber());
        case ZCCIROpcode::CMP_GT:
            return RuntimeValue(op0().asNumber() >  op1().asNumber());
        case ZCCIROpcode::CMP_GE:
            return RuntimeValue(op0().asNumber() >= op1().asNumber());
            
        // === 逻辑 ===
        case ZCCIROpcode::AND:
            return RuntimeValue(op0().asBool() && op1().asBool());
        case ZCCIROpcode::OR:
            return RuntimeValue(op0().asBool() || op1().asBool());
        case ZCCIROpcode::NOT:
            return RuntimeValue(!op0().asBool());
            
        // === 位运算 ===
        case ZCCIROpcode::SHL:
            return RuntimeValue(static_cast<int64_t>(op0().asNumber()) << static_cast<int>(op1().asNumber()));
        case ZCCIROpcode::SHR:
            return RuntimeValue(static_cast<int64_t>(op0().asNumber()) >> static_cast<int>(op1().asNumber()));
        case ZCCIROpcode::XOR:
            return RuntimeValue(static_cast<int64_t>(op0().asNumber()) ^ static_cast<int64_t>(op1().asNumber()));
            
        // === 数组 ===
        case ZCCIROpcode::ARRAY_NEW:
            return RuntimeValue(std::vector<RuntimeValue>{});
        case ZCCIROpcode::ARRAY_LEN: {
            auto& arr = op0().get<std::vector<RuntimeValue>>();
            return RuntimeValue(static_cast<int64_t>(arr.size()));
        }
        case ZCCIROpcode::ARRAY_GET: {
            auto& arr = op0().get<std::vector<RuntimeValue>>();
            size_t idx = static_cast<size_t>(op1().asNumber());
            return (idx < arr.size()) ? arr[idx] : RuntimeValue();
        }
        case ZCCIROpcode::ARRAY_SET: {
            auto& arr = op0().get<std::vector<RuntimeValue>>();
            size_t idx = static_cast<size_t>(op1().asNumber());
            if (idx >= arr.size()) arr.resize(idx + 1);
            arr[idx] = op2();
            return op2();
        }
            
        // === 对象 ===
        case ZCCIROpcode::OBJECT_NEW:
            return RuntimeValue(std::unordered_map<std::string, RuntimeValue>{});
        case ZCCIROpcode::OBJECT_GET: {
            auto& obj = op0().get<std::unordered_map<std::string, RuntimeValue>>();
            auto it = obj.find(op1().toString());
            return (it != obj.end()) ? it->second : RuntimeValue();
        }
        case ZCCIROpcode::OBJECT_SET: {
            auto& obj = op0().get<std::unordered_map<std::string, RuntimeValue>>();
            obj[op1().toString()] = op2();
            return op2();
        }
            
        // === 字符串 ===
        case ZCCIROpcode::STR_CONCAT:
            return RuntimeValue(op0().toString() + op1().toString());
        case ZCCIROpcode::STR_LEN: {
            return RuntimeValue(static_cast<int64_t>(op0().toString().size()));
        }
        case ZCCIROpcode::STR_CMP: {
            return RuntimeValue(static_cast<int64_t>(op0().toString() == op1().toString() ? 0 : 1));
        }
        case ZCCIROpcode::STR_SLICE: {
            auto s = op0().toString();
            int start = static_cast<int>(op1().asNumber());
            int end = static_cast<int>(op2().asNumber());
            if (start < 0) start = 0;
            if (end < 0 || end > (int)s.size()) end = (int)s.size();
            return RuntimeValue(s.substr(start, end - start));
        }
            
        // === 函数调用 ===
        case ZCCIROpcode::CALL:
            return executeCall(instr);
            
        // === Phi ===
        case ZCCIROpcode::PHI:
            return op0();
            
        // === 内存操作 ===
        case ZCCIROpcode::LOAD: {
            auto& operand = instr.getOperand(0);
            auto valPtr = operand.getValue();
            if (!valPtr) return RuntimeValue();
            
            auto it = reg_.find(valPtr->getID());
            if (it == reg_.end()) return RuntimeValue();
            
            RuntimeValue& rv = it->second;
            // 堆句柄解引用: LOAD handle [, offset] → 从 buffer 读 int64_t
            if (rv.isNumber()) {
                int64_t h = static_cast<int64_t>(rv.asNumber());
                auto at = allocTable_.find(h);
                if (at != allocTable_.end()) {
                    size_t off = (instr.getNumOperands() >= 2) 
                        ? static_cast<size_t>(op1().asNumber()) : 0;
                    if (off + 8 > at->second.size()) {
                        std::cerr << "ZCC: LOAD out of bounds (handle=" << h 
                                  << " offset=" << off << " size=" << at->second.size() << ")\n";
                        return RuntimeValue();
                    }
                    int64_t v = 0;
                    memcpy(&v, at->second.data() + off, 8);
                    return RuntimeValue(v);
                }
            }
            return rv;
        }
        case ZCCIROpcode::STORE: {
            auto& operand = instr.getOperand(0);
            auto valPtr = operand.getValue();
            if (!valPtr) return op1();
            
            auto it = reg_.find(valPtr->getID());
            // 堆句柄写入: STORE handle, offset, value (3 operands)
            if (it != reg_.end() && it->second.isNumber() && instr.getNumOperands() >= 3) {
                int64_t h = static_cast<int64_t>(it->second.asNumber());
                auto at = allocTable_.find(h);
                if (at != allocTable_.end()) {
                    size_t off = static_cast<size_t>(op1().asNumber());
                    auto val = (instr.getNumOperands() >= 3) ? op2() : op1();
                    if (off + 8 > at->second.size()) {
                        std::cerr << "ZCC: STORE out of bounds (handle=" << h
                                  << " offset=" << off << " size=" << at->second.size() << ")\n";
                        return RuntimeValue();
                    }
                    int64_t w = static_cast<int64_t>(val.asNumber());
                    memcpy(at->second.data() + off, &w, 8);
                    return val;
                }
            }
            // 普通变量写入: STORE var, value (2 operands)
            if (it != reg_.end()) {
                it->second = op1();
                return it->second;
            }
            reg_[valPtr->getID()] = op1();
            return op1();
        }
        case ZCCIROpcode::ALLOCA:
        case ZCCIROpcode::ALLOC: {
            size_t sz = static_cast<size_t>(op0().asNumber());
            if (sz == 0) sz = 8;
            int64_t handle = nextHandle_++;
            try {
                allocTable_[handle] = std::vector<uint8_t>(sz, 0);
            } catch (const std::bad_alloc&) {
                std::cerr << "ZCC: ALLOC failed, out of memory (size=" << sz << ")\n";
                return RuntimeValue(int64_t(-1));
            } catch (const std::length_error&) {
                std::cerr << "ZCC: ALLOC failed, size too large (size=" << sz << ")\n";
                return RuntimeValue(int64_t(-1));
            } catch (...) {
                std::cerr << "ZCC: ALLOC failed (size=" << sz << ")\n";
                return RuntimeValue(int64_t(-1));
            }
            return RuntimeValue(handle);
        }
        case ZCCIROpcode::FREE: {
            int64_t h = static_cast<int64_t>(op0().asNumber());
            auto it = allocTable_.find(h);
            if (it == allocTable_.end()) {
                std::cerr << "ZCC: FREE invalid handle " << h << "\n";
                return RuntimeValue();
            }
            allocTable_.erase(it);
            return RuntimeValue();
        }
            
        // === 自增/自减 ===
        case ZCCIROpcode::INC: {
            auto v = op0();
            double n = v.asNumber() + 1.0;
            auto& operand = instr.getOperand(0);
            auto valPtr = operand.getValue();
            if (valPtr) {
                RuntimeValue rv(n);
                reg_[valPtr->getID()] = rv;
                return rv;
            }
            return RuntimeValue(n);
        }
        case ZCCIROpcode::DEC: {
            auto v = op0();
            double n = v.asNumber() - 1.0;
            auto& operand = instr.getOperand(0);
            auto valPtr = operand.getValue();
            if (valPtr) {
                RuntimeValue rv(n);
                reg_[valPtr->getID()] = rv;
                return rv;
            }
            return RuntimeValue(n);
        }
            
        // === 类型转换 ===
        case ZCCIROpcode::TRUNC:
        case ZCCIROpcode::ZEXT:
        case ZCCIROpcode::SEXT:
            return RuntimeValue(static_cast<int64_t>(op0().asNumber()));
        case ZCCIROpcode::FPTRUNC:
        case ZCCIROpcode::FPEXT:
        case ZCCIROpcode::UITOFP:
        case ZCCIROpcode::SITOFP:
            return RuntimeValue(op0().asNumber());
        case ZCCIROpcode::FPTOUI:
        case ZCCIROpcode::FPTOSI:
            return RuntimeValue(static_cast<int64_t>(op0().asNumber()));
        case ZCCIROpcode::BITCAST:
            return op0();
            
        // === 高精度运算 (BigNum — 字符串实现) ===
        case ZCCIROpcode::BIG_ADD:
            return RuntimeValue(bignumAdd(op0().toString(), op1().toString()));
        case ZCCIROpcode::BIG_SUB:
            return RuntimeValue(bignumSub(op0().toString(), op1().toString()));
        case ZCCIROpcode::BIG_MUL:
            return RuntimeValue(bignumMul(op0().toString(), op1().toString()));
        case ZCCIROpcode::BIG_DIV:
            return RuntimeValue(bignumDiv(op0().toString(), op1().toString()));
        case ZCCIROpcode::BIG_MOD:
            return RuntimeValue(bignumMod(op0().toString(), op1().toString()));
        case ZCCIROpcode::BIG_CMP:
            return RuntimeValue(static_cast<int64_t>(bignumCmp(op0().toString(), op1().toString())));
        case ZCCIROpcode::BIG_POW:
            return RuntimeValue(bignumPow(op0().toString(), op1().asNumber()));
            
        // === 异常 + 特殊 ===
        case ZCCIROpcode::THROW: {
            std::cerr << "ZCC Exception: " << op0().toString() << "\n";
            return RuntimeValue();
        }
        case ZCCIROpcode::TRAP:
        case ZCCIROpcode::UNREACHABLE:
            std::cerr << "ZCC Interpreter: Unreachable trap\n";
            return RuntimeValue();
        case ZCCIROpcode::INTRINSIC:
        case ZCCIROpcode::ASSEMBLY:
            return RuntimeValue();  // 暂不支持
            
        default:
            std::cerr << "Unknown opcode: " << static_cast<int>(instr.getOpcode()) << "\n";
            return RuntimeValue();
    }
}

// === 操作数解析 ===
RuntimeValue NativeBackend::Interpreter::resolveOperand(const ZCCIRInstruction& instr, size_t idx) {
    if (idx >= instr.getNumOperands()) return RuntimeValue();
    
    auto& operand = instr.getOperand(idx);
    auto valPtr = operand.getValue();
    if (!valPtr) return RuntimeValue();
    
    // 常量 → RuntimeValue
    if (auto* constant = dynamic_cast<const ZCCIRConstant*>(valPtr.get())) {
        return constantToRuntime(constant->getValue());
    }
    
    // 指令结果 → 从寄存器读取
    auto id = valPtr->getID();
    auto it = reg_.find(id);
    if (it != reg_.end()) return it->second;
    
    // 变量引用 → 从作用域查找 (STORE/LOAD 的变量 ID 也在 reg_ 中)
    // 参数 / LOCAL 值 — 如果 reg_ 中有则直接取
    return RuntimeValue();
}

// === 常量转换 ===
RuntimeValue NativeBackend::Interpreter::constantToRuntime(
    const ZCCIRConstant::ConstantValue& cv) {
    if (std::holds_alternative<std::monostate>(cv)) return RuntimeValue();
    if (auto* b = std::get_if<bool>(&cv)) return RuntimeValue(*b);
    if (auto* i = std::get_if<int64_t>(&cv)) return RuntimeValue(*i);
    if (auto* d = std::get_if<double>(&cv)) return RuntimeValue(*d);
    if (auto* s = std::get_if<std::string>(&cv)) return RuntimeValue(*s);
    return RuntimeValue();
}

// === 函数调用 ===
RuntimeValue NativeBackend::Interpreter::executeCall(const ZCCIRInstruction& instr) {
    if (instr.getNumOperands() < 1) return RuntimeValue();
    
    // 收集参数
    std::vector<RuntimeValue> args;
    for (size_t i = 1; i < instr.getNumOperands(); ++i) {
        args.push_back(resolveOperand(instr, i));
    }
    
    // 查找内置函数（尝试已知名称）
    static const char* knownBuiltins[] = {
        "println", "print", "readline", "input", "typeof", "len", "str", "num", "exit"
    };
    for (auto* name : knownBuiltins) {
        auto* builtin = state_.findBuiltin(name);
        if (builtin) {
            return (*builtin)(args);
        }
    }
    
    // IR 函数调用（查找已注册的函数并递归执行）
    // TODO: 完整实现需要遍历 module->getFunctions() 找到匹配的函数并递归执行
    
    return RuntimeValue();
}

// ==================== 原生后端实现 ====================

CompileResult NativeBackend::compile(const ZCCIRModule& module,
                                     const CompileOptions& options) {
    CompileResult result;
    result.success = true;
    
    try {
        // 1. 保存 IR 文件
        std::string irPath = options.outputPath + ".ir";
        if (!saveIRToFile(module, irPath)) {
            result.success = false;
            result.errorMessage = "Failed to save IR file";
            return result;
        }
        
        // 2. 生成自包含的可执行文件
        result.binary = generateExecutable(module, options);
        
        // 3. 保存可执行文件
        if (!options.outputPath.empty() && !result.binary.empty()) {
            std::ofstream outFile(options.outputPath, std::ios::binary);
            outFile.write(reinterpret_cast<const char*>(result.binary.data()), 
                         result.binary.size());
            outFile.close();
        }
        
        result.output = "Generated: " + options.outputPath + " and " + irPath;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

std::vector<uint8_t> NativeBackend::generateExecutable(const ZCCIRModule& module,
                                                       const CompileOptions& options) {
    // 生成自包含的可执行文件
    // 格式：[解释器代码][IR 数据][IR 数据大小][入口点]
    
    std::vector<uint8_t> executable;
    
    // 这里应该嵌入解释器代码和 IR 数据
    // 为了简化，我们生成一个包装器脚本
    
    std::ostringstream wrapper;
    
#ifdef _WIN32
    // Windows 批处理包装器
    wrapper << "@echo off\n";
    wrapper << "REM ZCC Native Executable\n";
    wrapper << "REM Generated by ZCC Compiler\n\n";
    wrapper << "set ZCC_IR_FILE=" << options.outputPath << ".ir\n";
    wrapper << "zcc run \"%ZCC_IR_FILE%\" %*\n";
    
    std::string wrapperStr = wrapper.str();
    executable.assign(wrapperStr.begin(), wrapperStr.end());
    
#else
    // Linux Shell 包装器
    wrapper << "#!/bin/bash\n";
    wrapper << "# ZCC Native Executable\n";
    wrapper << "# Generated by ZCC Compiler\n\n";
    wrapper << "SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n";
    wrapper << "ZCC_IR_FILE=\"${SCRIPT_DIR}/" << module.getName() << ".ir\"\n";
    wrapper << "zcc run \"$ZCC_IR_FILE\" \"$@\"\n";
    
    std::string wrapperStr = wrapper.str();
    executable.assign(wrapperStr.begin(), wrapperStr.end());
#endif
    
    return executable;
}

bool NativeBackend::saveIRFile(const ZCCIRModule& module, const std::string& path) {
    std::ofstream file(path);
    if (!file) return false;
    
    file << "; ZCC-IR File\n";
    file << "; Generated by ZCC Compiler\n";
    file << ".version 1.0\n";
    file << ".module \"" << module.getName() << "\"\n\n";
    
    // 全局变量
    for (const auto& global : module.getGlobals()) {
        file << ".global @" << global->getName();
        if (global->getInitializer()) {
            file << " = " << global->getInitializer()->toString();
        }
        file << "\n";
    }
    file << "\n";
    
    // 函数
    for (const auto& func : module.getFunctions()) {
        if (func->isExternal()) continue;
        
        file << ".func ";
        file << func->getReturnType()->toString() << " @" << func->getName() << "(";
        
        const auto& args = func->getArguments();
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) file << ", ";
            file << args[i].second->toString() << " %" << args[i].first;
        }
        file << ") {\n";
        
        for (const auto& block : func->getBlocks()) {
            file << "." << block->getLabel() << ":\n";
            
            for (const auto& instr : block->getInstructions()) {
                file << "    " << instr->toString() << "\n";
            }
        }
        
        file << "}\n\n";
    }
    
    file.close();
    return true;
}

// ==================== 运行 IR 模块 ====================

RuntimeValue NativeBackend::runModule(const ZCCIRModule& module) {
    InterpreterState state;
    Interpreter interpreter(state);
    
    // 注册函数
    for (const auto& func : module.getFunctions()) {
        state.setFunction(func->getName(), func.get());
    }
    
    return interpreter.execute();
}

// ==================== IR 文件保存/加载 ====================

bool saveIRToFile(const ZCCIRModule& module, const std::string& filename) {
    NativeBackend backend;
    return backend.saveIRFile(module, filename);
}

std::unique_ptr<ZCCIRModule> loadIRFromFile(const std::string& filename) {
    // TODO: 实现 IR 文件解析
    return nullptr;
}

} // namespace zcc
