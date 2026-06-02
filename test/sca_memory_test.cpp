// SCA 内存控制指令完整测试
#define ZCC_TEST_ACCESS
#include "NativeBackend.hpp"
#include "ZCCIR.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
using namespace zcc;

int main() {
    int passed = 0, failed = 0;
    auto pass = [&](const char* n) { passed++; std::cout << "  [PASS] " << n << "\n"; };
    auto fail = [&](const char* n) { failed++; std::cout << "  [FAIL] " << n << "\n"; };
    auto check = [&](bool c, const char* n) { c ? pass(n) : fail(n); };

    std::cout << "=== SCA 内存控制指令测试 ===\n\n";

    InterpreterState state;
    NativeBackend::Interpreter interp(state);
    auto& reg = interp.testReg();
    auto& allocTable = interp.testAllocTable();

    // 模拟 executeBlock 的 reg_ 更新
    auto exec = [&](std::unique_ptr<ZCCIRInstruction>& instr) -> RuntimeValue {
        auto rv = interp.executeInstruction(*instr);
        auto rp = instr->getResult(); if (rp) reg[rp->getID()] = rv;
        return rv;
    };

    auto make = [](ZCCIROpcode op, std::vector<ZCCIROperand> ops, ZCCTypePtr ty = nullptr) {
        if (!ty) ty = TypeManager::getInstance().getI64();
        static uint32_t nid = 1;
        auto i = std::make_unique<ZCCIRInstruction>(op, ty, nid++);
        // Set result for reg_ tracking
        auto rv = std::make_shared<ZCCIRValue>(ZCCIRValueKind::TEMPORARY, ty, 1000+nid);
        i->setResult(rv);
        for (auto& o : ops) i->addOperand(std::move(o));
        return i;
    };

    auto C = [](int64_t v) {
        auto c = std::make_shared<ZCCIRConstant>(ZCCIRConstant::ConstantValue(v), 
                   TypeManager::getInstance().getI64(), 100+v);
        return ZCCIROperand(c);
    };

    auto V = [](uint32_t id) {
        auto v = std::make_shared<ZCCIRValue>(ZCCIRValueKind::LOCAL, 
                  TypeManager::getInstance().getI64(), id);
        return ZCCIROperand(v);
    };

    // ==== 1. ALLOC 正常 ====
    std::cout << "1. ALLOC 正常\n";
    { auto i=make(ZCCIROpcode::ALLOC,{C(1024)}); int64_t h=(int64_t)exec(i).asNumber();
      check(h>0,"ALLOC(1024)>0"); check(h!=-1,"不是-1");
      check(allocTable.find(h)!=allocTable.end(),"在 allocTable"); check(allocTable[h].size()==1024,"1024B"); }

    // ==== 2. ALLOC(0) ====
    std::cout << "\n2. ALLOC(0)\n";
    { auto i=make(ZCCIROpcode::ALLOC,{C(0)}); int64_t h=(int64_t)exec(i).asNumber();
      check(h>0,"有效句柄"); check(allocTable[h].size()>=8,"min 8B"); }

    // ==== 3. STORE/LOAD offset 0 ====
    std::cout << "\n3. STORE/LOAD off=0\n";
    { auto a=make(ZCCIROpcode::ALLOC,{C(64)}); int64_t h=(int64_t)exec(a).asNumber();
      reg[2001]=RuntimeValue(h);
      auto s=make(ZCCIROpcode::STORE,{V(2001),C(0),C(42)}); exec(s);
      auto l=make(ZCCIROpcode::LOAD,{V(2001),C(0)}); int64_t v=(int64_t)exec(l).asNumber();
      check(v==42,"42"); }

    // ==== 4. 非零偏移 ====
    std::cout << "\n4. 非零偏移\n";
    { auto a=make(ZCCIROpcode::ALLOC,{C(64)}); int64_t h=(int64_t)exec(a).asNumber();
      reg[2002]=RuntimeValue(h);
      auto s8=make(ZCCIROpcode::STORE,{V(2002),C(8),C(77)}); exec(s8);
      auto s16=make(ZCCIROpcode::STORE,{V(2002),C(16),C(99)}); exec(s16);
      auto l0=make(ZCCIROpcode::LOAD,{V(2002),C(0)}); check((int64_t)exec(l0).asNumber()==0,"off0=0");
      auto l8=make(ZCCIROpcode::LOAD,{V(2002),C(8)}); check((int64_t)exec(l8).asNumber()==77,"off8=77");
      auto l16=make(ZCCIROpcode::LOAD,{V(2002),C(16)}); check((int64_t)exec(l16).asNumber()==99,"off16=99"); }

    // ==== 5. FREE ====
    std::cout << "\n5. FREE\n";
    { auto a=make(ZCCIROpcode::ALLOC,{C(32)}); int64_t h=(int64_t)exec(a).asNumber();
      check(allocTable.find(h)!=allocTable.end(),"FREE前存在");
      auto f=make(ZCCIROpcode::FREE,{C(h)}); exec(f);
      check(allocTable.find(h)==allocTable.end(),"FREE后移除");
      exec(f); pass("双重FREE不崩"); } // cerr expected

    // ==== 6. 越界 ====
    std::cout << "\n6. 越界\n";
    { auto a=make(ZCCIROpcode::ALLOC,{C(16)}); int64_t h=(int64_t)exec(a).asNumber();
      reg[2003]=RuntimeValue(h);
      auto sb=make(ZCCIROpcode::STORE,{V(2003),C(64),C(1)}); check(exec(sb).isNull(),"STORE越界null");
      auto lb=make(ZCCIROpcode::LOAD,{V(2003),C(64)}); check(exec(lb).isNull(),"LOAD越界null"); }

    // ==== 7. OOM ====
    std::cout << "\n7. OOM\n";
    { auto i=make(ZCCIROpcode::ALLOC,{C((int64_t)SIZE_MAX)}); int64_t h=(int64_t)exec(i).asNumber();
      check(h==-1,"ALLOC(SIZE_MAX)=>-1"); }

    // ==== 8. 负偏移 ====
    std::cout << "\n8. 负偏移\n";
    { auto a=make(ZCCIROpcode::ALLOC,{C(32)}); int64_t h=(int64_t)exec(a).asNumber();
      reg[2004]=RuntimeValue(h);
      auto l=make(ZCCIROpcode::LOAD,{V(2004),C((int64_t)-1)}); check(exec(l).isNull(),"负偏移null"); }

    std::cout << "\n=========================\n";
    std::cout << passed << "/" << (passed+failed) << " PASS\n";
    return failed ? 1 : 0;
}
