#ifndef ZCC_KEYWORDS_HPP
#define ZCC_KEYWORDS_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace zcc {

// 中文关键词
namespace CNKeywords {
    inline const std::string 定义 = "定义";
    inline const std::string 常量 = "常量";
    inline const std::string 函数 = "函数";
    inline const std::string 主函数 = "主函数";
    inline const std::string 返回 = "返回";
    inline const std::string 如果 = "如果";
    inline const std::string 否则 = "否则";
    inline const std::string 否则如果 = "否则如果";
    inline const std::string 循环 = "循环";
    inline const std::string 当 = "当";
    inline const std::string 中断 = "中断";
    inline const std::string 继续 = "继续";
    inline const std::string 导入 = "导入";
    inline const std::string 导出 = "导出";
    inline const std::string 尝试 = "尝试";
    inline const std::string 捕获 = "捕获";
    inline const std::string 最终 = "最终";
    inline const std::string 抛出 = "抛出";
    inline const std::string 是 = "是";
    inline const std::string 否 = "否";
    inline const std::string 空 = "空";
    inline const std::string 未定义 = "未定义";
    inline const std::string 新 = "新";
    inline const std::string 类 = "类";
    inline const std::string 继承 = "继承";
    inline const std::string 构造 = "构造";
    inline const std::string 自身 = "自身";
    inline const std::string 极 = "极";
    inline const std::string 静态 = "静态";
    inline const std::string 获取 = "获取";
    inline const std::string 设置 = "设置";
    inline const std::string 模块 = "模块";
    inline const std::string 每个 = "每个";
    inline const std::string 在 = "在";
    inline const std::string 中 = "中";
    inline const std::string 从 = "从";
    inline const std::string 为 = "为";
    inline const std::string 默认 = "默认";
    inline const std::string 异步 = "异步";
    inline const std::string 等待 = "等待";
    inline const std::string 承诺 = "承诺";
    inline const std::string 解决 = "解决";
    inline const std::string 拒绝 = "拒绝";
    inline const std::string 线程 = "线程";
    inline const std::string 启动 = "启动";
    inline const std::string 睡眠 = "睡眠";
    inline const std::string 等待所有 = "等待所有";
    inline const std::string 关闭 = "关闭";
    inline const std::string 切换 = "切换";
    inline const std::string 情况 = "情况";
    inline const std::string 枚举 = "枚举";
    inline const std::string 命名空间 = "命名空间";
}

// 英文关键词
namespace ENKeywords {
    inline const std::string auto_ = "auto";
    inline const std::string const_ = "const";
    inline const std::string function = "function";
    inline const std::string main = "main";
    inline const std::string return_ = "return";
    inline const std::string if_ = "if";
    inline const std::string else_ = "else";
    inline const std::string elseif = "elseif";
    inline const std::string for_ = "for";
    inline const std::string while_ = "while";
    inline const std::string break_ = "break";
    inline const std::string continue_ = "continue";
    inline const std::string import = "import";
    inline const std::string export_ = "export";
    inline const std::string try_ = "try";
    inline const std::string catch_ = "catch";
    inline const std::string finally = "finally";
    inline const std::string throw_ = "throw";
    inline const std::string true_ = "true";
    inline const std::string false_ = "false";
    inline const std::string null = "null";
    inline const std::string undefined = "undefined";
    inline const std::string new_ = "new";
    inline const std::string class_ = "class";
    inline const std::string extends = "extends";
    inline const std::string constructor = "constructor";
    inline const std::string this_ = "this";
    inline const std::string super = "super";
    inline const std::string static_ = "static";
    inline const std::string get = "get";
    inline const std::string set = "set";
    inline const std::string module = "module";
    inline const std::string each = "each";
    inline const std::string in = "in";
    inline const std::string as = "as";
    inline const std::string from = "from";
    inline const std::string default_ = "default";
    inline const std::string async = "async";
    inline const std::string await = "await";
    inline const std::string promise = "promise";
    inline const std::string resolve = "resolve";
    inline const std::string reject = "reject";
    inline const std::string thread = "thread";
    inline const std::string start = "start";
    inline const std::string sleep = "sleep";
    inline const std::string waitAll = "waitAll";
    inline const std::string close = "close";
    inline const std::string switch_ = "switch";
    inline const std::string case_ = "case";
    inline const std::string enum_ = "enum";
    inline const std::string namespace_ = "namespace";
}

// 关键词映射
class KeywordMap {
public:
    enum class KeywordType {
        // 变量和常量
        AUTO, CONST,
        // 函数
        FUNCTION, MAIN, RETURN,
        // 控制流
        IF, ELSE, ELSEIF, FOR, WHILE, BREAK, CONTINUE,
        // 模块
        IMPORT, EXPORT, MODULE, FROM, AS, DEFAULT_,
        // 异常
        TRY, CATCH, FINALLY, THROW,
        // 字面量
        TRUE, FALSE, NULL_, UNDEFINED,
        // 面向对象
        NEW, CLASS, EXTENDS, CONSTRUCTOR, THIS, SUPER, STATIC_, GET, SET,
        // 遍历
        EACH, IN,
        // 异步
        ASYNC, AWAIT, PROMISE, RESOLVE, REJECT,
        // 线程
        THREAD, START, SLEEP, WAIT_ALL, CLOSE,
        // 其他
        SWITCH, CASE,
        FUNC,
        ENUM, NAMESPACE,
        // 不是关键词
        NOT_KEYWORD
    };

private:
    std::unordered_map<std::string, KeywordType> keywordMap_;

public:
    KeywordMap() {
        // 中文关键词
        keywordMap_[CNKeywords::定义] = KeywordType::AUTO;
        keywordMap_[CNKeywords::常量] = KeywordType::CONST;
        keywordMap_[CNKeywords::函数] = KeywordType::FUNCTION;
        keywordMap_[CNKeywords::主函数] = KeywordType::MAIN;
        keywordMap_[CNKeywords::返回] = KeywordType::RETURN;
        keywordMap_[CNKeywords::如果] = KeywordType::IF;
        keywordMap_[CNKeywords::否则] = KeywordType::ELSE;
        keywordMap_[CNKeywords::否则如果] = KeywordType::ELSEIF;
        keywordMap_[CNKeywords::循环] = KeywordType::FOR;
        keywordMap_[CNKeywords::当] = KeywordType::WHILE;
        keywordMap_[CNKeywords::中断] = KeywordType::BREAK;
        keywordMap_[CNKeywords::继续] = KeywordType::CONTINUE;
        keywordMap_[CNKeywords::导入] = KeywordType::IMPORT;
        keywordMap_[CNKeywords::导出] = KeywordType::EXPORT;
        keywordMap_[CNKeywords::模块] = KeywordType::MODULE;
        keywordMap_[CNKeywords::从] = KeywordType::FROM;
        keywordMap_[CNKeywords::为] = KeywordType::AS;
        keywordMap_[CNKeywords::默认] = KeywordType::DEFAULT_;
        keywordMap_[CNKeywords::尝试] = KeywordType::TRY;
        keywordMap_[CNKeywords::捕获] = KeywordType::CATCH;
        keywordMap_[CNKeywords::最终] = KeywordType::FINALLY;
        keywordMap_[CNKeywords::抛出] = KeywordType::THROW;
        keywordMap_[CNKeywords::是] = KeywordType::TRUE;
        keywordMap_[CNKeywords::否] = KeywordType::FALSE;
        keywordMap_[CNKeywords::空] = KeywordType::NULL_;
        keywordMap_[CNKeywords::未定义] = KeywordType::UNDEFINED;
        keywordMap_[CNKeywords::新] = KeywordType::NEW;
        keywordMap_[CNKeywords::类] = KeywordType::CLASS;
        keywordMap_[CNKeywords::继承] = KeywordType::EXTENDS;
        keywordMap_[CNKeywords::构造] = KeywordType::CONSTRUCTOR;
        keywordMap_[CNKeywords::自身] = KeywordType::THIS;
        keywordMap_[CNKeywords::极] = KeywordType::SUPER;
        keywordMap_[CNKeywords::静态] = KeywordType::STATIC_;
        keywordMap_[CNKeywords::获取] = KeywordType::GET;
        keywordMap_[CNKeywords::设置] = KeywordType::SET;
        keywordMap_[CNKeywords::每个] = KeywordType::EACH;
        keywordMap_[CNKeywords::在] = KeywordType::IN;
        keywordMap_[CNKeywords::异步] = KeywordType::ASYNC;
        keywordMap_[CNKeywords::等待] = KeywordType::AWAIT;
        keywordMap_[CNKeywords::承诺] = KeywordType::PROMISE;
        keywordMap_[CNKeywords::解决] = KeywordType::RESOLVE;
        keywordMap_[CNKeywords::拒绝] = KeywordType::REJECT;
        keywordMap_[CNKeywords::线程] = KeywordType::THREAD;
        keywordMap_[CNKeywords::启动] = KeywordType::START;
        keywordMap_[CNKeywords::睡眠] = KeywordType::SLEEP;
        keywordMap_[CNKeywords::等待所有] = KeywordType::WAIT_ALL;
        keywordMap_[CNKeywords::关闭] = KeywordType::CLOSE;
        keywordMap_[CNKeywords::切换] = KeywordType::SWITCH;
        keywordMap_[CNKeywords::情况] = KeywordType::CASE;
        keywordMap_[CNKeywords::枚举] = KeywordType::ENUM;
        keywordMap_[CNKeywords::命名空间] = KeywordType::NAMESPACE;

        // 英文关键词
        keywordMap_[ENKeywords::auto_] = KeywordType::AUTO;
        keywordMap_[ENKeywords::const_] = KeywordType::CONST;
        keywordMap_[ENKeywords::function] = KeywordType::FUNCTION;
        keywordMap_[ENKeywords::main] = KeywordType::MAIN;
        keywordMap_[ENKeywords::return_] = KeywordType::RETURN;
        keywordMap_[ENKeywords::if_] = KeywordType::IF;
        keywordMap_[ENKeywords::else_] = KeywordType::ELSE;
        keywordMap_[ENKeywords::elseif] = KeywordType::ELSEIF;
        keywordMap_[ENKeywords::for_] = KeywordType::FOR;
        keywordMap_[ENKeywords::while_] = KeywordType::WHILE;
        keywordMap_[ENKeywords::break_] = KeywordType::BREAK;
        keywordMap_[ENKeywords::continue_] = KeywordType::CONTINUE;
        keywordMap_[ENKeywords::import] = KeywordType::IMPORT;
        keywordMap_[ENKeywords::export_] = KeywordType::EXPORT;
        keywordMap_[ENKeywords::module] = KeywordType::MODULE;
        keywordMap_[ENKeywords::from] = KeywordType::FROM;
        keywordMap_[ENKeywords::as] = KeywordType::AS;
        keywordMap_[ENKeywords::default_] = KeywordType::DEFAULT_;
        keywordMap_[ENKeywords::try_] = KeywordType::TRY;
        keywordMap_[ENKeywords::catch_] = KeywordType::CATCH;
        keywordMap_[ENKeywords::finally] = KeywordType::FINALLY;
        keywordMap_[ENKeywords::throw_] = KeywordType::THROW;
        keywordMap_[ENKeywords::true_] = KeywordType::TRUE;
        keywordMap_[ENKeywords::false_] = KeywordType::FALSE;
        keywordMap_[ENKeywords::null] = KeywordType::NULL_;
        keywordMap_[ENKeywords::undefined] = KeywordType::UNDEFINED;
        keywordMap_[ENKeywords::new_] = KeywordType::NEW;
        keywordMap_[ENKeywords::class_] = KeywordType::CLASS;
        keywordMap_[ENKeywords::extends] = KeywordType::EXTENDS;
        keywordMap_[ENKeywords::constructor] = KeywordType::CONSTRUCTOR;
        keywordMap_[ENKeywords::this_] = KeywordType::THIS;
        keywordMap_[ENKeywords::super] = KeywordType::SUPER;
        keywordMap_[ENKeywords::static_] = KeywordType::STATIC_;
        keywordMap_[ENKeywords::get] = KeywordType::GET;
        keywordMap_[ENKeywords::set] = KeywordType::SET;
        keywordMap_[ENKeywords::each] = KeywordType::EACH;
        keywordMap_[ENKeywords::in] = KeywordType::IN;
        keywordMap_[ENKeywords::async] = KeywordType::ASYNC;
        keywordMap_[ENKeywords::await] = KeywordType::AWAIT;
        keywordMap_[ENKeywords::promise] = KeywordType::PROMISE;
        keywordMap_[ENKeywords::resolve] = KeywordType::RESOLVE;
        keywordMap_[ENKeywords::reject] = KeywordType::REJECT;
        keywordMap_[ENKeywords::thread] = KeywordType::THREAD;
        keywordMap_[ENKeywords::start] = KeywordType::START;
        keywordMap_[ENKeywords::sleep] = KeywordType::SLEEP;
        keywordMap_[ENKeywords::waitAll] = KeywordType::WAIT_ALL;
        keywordMap_[ENKeywords::close] = KeywordType::CLOSE;
        keywordMap_[ENKeywords::switch_] = KeywordType::SWITCH;
        keywordMap_[ENKeywords::case_] = KeywordType::CASE;
        keywordMap_[ENKeywords::enum_] = KeywordType::ENUM;
        keywordMap_[ENKeywords::namespace_] = KeywordType::NAMESPACE;
    }

    KeywordType lookup(const std::string& identifier) const {
        auto it = keywordMap_.find(identifier);
        if (it != keywordMap_.end()) {
            return it->second;
        }
        return KeywordType::NOT_KEYWORD;
    }

    bool isKeyword(const std::string& identifier) const {
        return lookup(identifier) != KeywordType::NOT_KEYWORD;
    }
};

// 全局关键词映射实例 - 使用函数返回静态局部变量确保初始化顺序
inline KeywordMap& getKeywordMap() {
    static KeywordMap instance;
    return instance;
}

} // namespace zcc

#endif // ZCC_KEYWORDS_HPP
