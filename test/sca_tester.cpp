// SCA v0.3 Manifest 测试 — 自包含编译
// 直接 include Module.cpp 的 SmartSCALoader 部分，通过宏跳过 IPC
#define ZCC_SCA_TEST_STANDALONE
#include "Module.hpp"
#include <iostream>
#include <cassert>

using namespace zcc;

// === 最小 IPC 桩 (类型签名需匹配 IPC.hpp) ===
namespace zcc {
    struct ConnectionPool { ~ConnectionPool() = default; };
    struct ConnectOptions { };
    class IPCManager {
    public:
        static IPCManager& getInstance() { static IPCManager i; return i; }
        uint32_t connect(const std::string& a, const ConnectOptions& o) { return 0; }
    };
}

// === SmartSCALoader 实现 (直接嵌入, 仅含 SCA 部分) ===
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <regex>
#include <set>
#include <queue>
#include <thread>
#include <chrono>

namespace zcc {

std::string trim_sca(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

SCAManifest parse_manifest_test(const std::string& content) {
    SCAManifest manifest;
    std::regex manifest_regex(R"(\[manifest\]([\s\S]*?)\[/manifest\])");
    std::smatch match;
    if (!std::regex_search(content, match, manifest_regex)) return manifest;
    
    manifest.has_manifest = true;
    std::string body = match[1].str();
    
    enum class Section { NONE, DEPENDENCY, INTERFACE };
    Section currentSection = Section::NONE;
    SCAManifestDependency currentDep;
    SCAManifestInterface currentIface;
    
    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim_sca(line);
        if (line.empty()) continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '*') continue;
        
        if (line == "[dependency]") {
            if (currentSection == Section::DEPENDENCY && !currentDep.name.empty())
                manifest.dependencies.push_back(std::move(currentDep));
            if (currentSection == Section::INTERFACE && !currentIface.symbol.empty())
                manifest.interfaces.push_back(std::move(currentIface));
            currentDep = {}; currentSection = Section::DEPENDENCY;
            continue;
        }
        if (line == "[interface]") {
            if (currentSection == Section::DEPENDENCY && !currentDep.name.empty())
                manifest.dependencies.push_back(std::move(currentDep));
            if (currentSection == Section::INTERFACE && !currentIface.symbol.empty())
                manifest.interfaces.push_back(std::move(currentIface));
            currentIface = {}; currentSection = Section::INTERFACE;
            continue;
        }
        
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = trim_sca(line.substr(0, eqPos));
        std::string value = trim_sca(line.substr(eqPos + 1));
        
        switch (currentSection) {
            case Section::NONE:
                if (key == "module.name")          manifest.name = value;
                else if (key == "module.version")  manifest.version = value;
                else if (key == "module.sca_version") manifest.sca_version = value;
                else if (key == "module.modes") {
                    std::istringstream ms(value);
                    std::string mode;
                    while (std::getline(ms, mode, ',')) {
                        mode = trim_sca(mode);
                        if (!mode.empty()) manifest.modes.push_back(mode);
                    }
                }
                break;
            case Section::DEPENDENCY:
                if (key == "name") currentDep.name = value;
                else if (key == "type") currentDep.type = value;
                else if (key == "version") currentDep.version = value;
                else if (key == "min_version") currentDep.min_version = value;
                else if (key == "ipc_method") currentDep.ipc_method = value;
                else if (key == "version_cmd") currentDep.version_cmd = value;
                break;
            case Section::INTERFACE:
                if (key == "symbol") currentIface.symbol = value;
                else if (key == "provider") currentIface.provider = value;
                else if (key == "export") currentIface.export_name = value;
                else if (key == "endpoint") currentIface.endpoint = value;
                break;
        }
    }
    
    if (currentSection == Section::DEPENDENCY && !currentDep.name.empty())
        manifest.dependencies.push_back(std::move(currentDep));
    if (currentSection == Section::INTERFACE && !currentIface.symbol.empty())
        manifest.interfaces.push_back(std::move(currentIface));
    
    if (manifest.modes.empty()) manifest.modes.push_back("compile");
    return manifest;
}
} // namespace zcc

void test_manifest_parsing() {
    std::cout << "=== Test 1: Manifest 解析 ===\n";
    
    std::ifstream f("test/sca_test_manifest.sca");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    if (content.size() >= 4 && content.substr(0, 4) == "ZCCS") content = content.substr(4);
    
    auto manifest = parse_manifest_test(content);
    assert(manifest.has_manifest);
    
    std::cout << "名称: " << manifest.name << "\n";
    std::cout << "版本: " << manifest.version << "\n";
    std::cout << "SCA版本: " << manifest.sca_version << "\n";
    std::cout << "模式: ";
    for (auto& m : manifest.modes) std::cout << m << " ";
    std::cout << "\n";
    std::cout << "compile: " << manifest.supportsCompile() << "\n";
    std::cout << "ir-exec: " << manifest.supportsIRExec() << "\n";
    std::cout << "依赖 (" << manifest.dependencies.size() << "):\n";
    for (auto& d : manifest.dependencies) {
        std::cout << "  - " << d.name << " [" << d.type << "] " << d.version << "\n";
    }
    std::cout << "接口映射 (" << manifest.interfaces.size() << "):\n";
    for (auto& i : manifest.interfaces) {
        std::cout << "  - " << i.symbol << " -> " << i.provider << "." << i.export_name;
        if (!i.endpoint.empty()) std::cout << " @" << i.endpoint;
        std::cout << "\n";
    }
    
    assert(manifest.name == "org.example.network");
    assert(manifest.version == "1.0.0");
    assert(manifest.sca_version == "0.3");
    assert(manifest.dependencies.size() == 2);
    assert(manifest.dependencies[0].name == "org.std.crypto");
    assert(manifest.dependencies[1].type == "executable");
    assert(manifest.interfaces.size() == 2);
    assert(manifest.interfaces[0].symbol == "hash");
    assert(manifest.supportsCompile());
    assert(manifest.supportsIRExec());
    std::cout << "PASSED\n\n";
}

void test_no_manifest() {
    std::cout << "=== Test 2: 无 Manifest (v0.2 兼容) ===\n";
    
    std::ifstream f("test/hello.zcc");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    auto manifest = parse_manifest_test(content);
    assert(!manifest.has_manifest);
    std::cout << "has_manifest: false (正确)\n";
    std::cout << "PASSED\n\n";
}

void test_forbidden_blocks() {
    std::cout << "=== Test 3: 禁止 [c] 块检测 ===\n";
    
    std::ifstream f("test/sca_test_manifest.sca");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    // 在 Manifest 中检查不应该有 [c] 块
    std::regex forbidden(R"(\[(c|cpp|c\+\+|asm|assembly)\])");
    assert(!std::regex_search(content, forbidden));
    std::cout << "无禁止块 (正确)\n";
    std::cout << "PASSED\n\n";
}

int main() {
    std::cout << "ZCC SCA v0.3 Manifest Test\n===========================\n\n";
    try {
        test_manifest_parsing();
        test_no_manifest();
        test_forbidden_blocks();
        std::cout << "===========================\nALL TESTS PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
