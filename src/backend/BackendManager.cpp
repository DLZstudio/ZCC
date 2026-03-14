#include "Backend.hpp"

namespace zcc {

std::vector<OptimizationLevel> Backend::getSupportedOptimizations() const {
    return {
        OptimizationLevel::O0,
        OptimizationLevel::O1,
        OptimizationLevel::O2,
        OptimizationLevel::O3
    };
}

BackendManager& BackendManager::getInstance() {
    static BackendManager instance;
    return instance;
}

void BackendManager::registerBackend(std::unique_ptr<Backend> backend) {
    if (backend) {
        backends_[backend->getName()] = std::move(backend);
    }
}

Backend* BackendManager::getBackend(const std::string& name) {
    auto it = backends_.find(name);
    if (it != backends_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> BackendManager::getAvailableBackends() const {
    std::vector<std::string> names;
    for (const auto& [name, backend] : backends_) {
        names.push_back(name);
    }
    return names;
}

Backend* BackendManager::selectBackendForTarget(const TargetPlatform& target) {
    // 根据目标平台选择后端
    if (target.isWindows()) {
        return getBackend("windows");
    } else if (target.isLinux()) {
        return getBackend("linux");
    } else if (target.isBIOS()) {
        return getBackend("bios");
    } else {
        // 默认使用当前平台对应的后端
        auto current = TargetPlatform::getCurrent();
        if (current.isWindows()) {
            return getBackend("windows");
        } else if (current.isLinux()) {
            return getBackend("linux");
        }
    }
    
    // 回退到 C 后端
    return getBackend("c");
}

CompileResult BackendManager::compile(const ZCCIRModule& module,
                                      const std::string& backendName,
                                      const CompileOptions& options) {
    Backend* backend = getBackend(backendName);
    if (!backend) {
        CompileResult result;
        result.success = false;
        result.errorMessage = "Backend not found: " + backendName;
        return result;
    }
    
    return backend->compile(module, options);
}

} // namespace zcc
