#ifndef ZCC_IPC_HPP
#define ZCC_IPC_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <future>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
#endif

namespace zcc {

// 前向声明
struct Value { std::string type; std::string data; Value() : type("null") {} Value(const std::string& d, const std::string& t) : type(t), data(d) {} explicit Value(int64_t i) : type("int"), data(std::to_string(i)) {} explicit Value(bool b) : type("bool"), data(b?"true":"false") {} bool isNull() const { return type == "null"; } std::string toString() const { return data; } };
class IPCChannel;

// ============== 基础类型 ==============

// 连接 ID
using ConnectionID = uint64_t;
using ServerID = uint64_t;
using StreamID = uint64_t;
using SubscriptionID = uint64_t;

// IPC 消息类型（扩展版）
enum class MessageType : uint8_t {
    // 基础消息
    REQUEST         = 0x01,
    RESPONSE        = 0x02,
    NOTIFICATION    = 0x03,
    IPC_ERROR       = 0x04,
    
    // 流控制
    STREAM_OPEN     = 0x10,
    STREAM_DATA     = 0x11,
    STREAM_CLOSE    = 0x12,
    STREAM_ACK      = 0x13,
    
    // 发布/订阅
    SUBSCRIBE       = 0x20,
    UNSUBSCRIBE     = 0x21,
    PUBLISH         = 0x22,
    
    // 心跳和保活
    HEARTBEAT       = 0x30,
    PING            = 0x31,
    PONG            = 0x32,
    
    // 服务发现
    DISCOVER        = 0x40,
    ADVERTISE       = 0x41,
    
    // 控制消息
    AUTH            = 0x50,
    FLOW_CONTROL    = 0x51,
    SHUTDOWN        = 0x52,
};

// IPC 错误代码
enum class IPCErrorCode : uint32_t {
    SUCCESS = 0,
    PROCESS_NOT_FOUND = 1,
    CONNECTION_FAILED = 2,
    TIMEOUT = 3,
    FUNCTION_NOT_FOUND = 4,
    INVALID_ARGUMENTS = 5,
    UNKNOWN_ERROR = 99,
};

// IPC 消息头 (20 字节)
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic = 0x5A4343;  // "ZCC"
    uint32_t version = 1;
    uint32_t message_type;
    uint32_t sequence_id;
    uint32_t body_size;
};
#pragma pack(pop)

// IPC 消息
struct Message {
    MessageHeader header;
    std::vector<uint8_t> body;
    std::string service;      // 服务名
    std::string method;       // 方法名
    std::vector<uint8_t> arguments;  // 参数
    std::vector<uint8_t> result;     // 结果
    std::string error_message;       // 错误消息
    uint64_t stream_id = 0;   // 流 ID
    std::string topic;        // 主题（发布/订阅）
    
    Message() {
        header.magic = 0x5A4343;
        header.version = 1;
    }
    
    // 序列化
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data(sizeof(MessageHeader) + body.size());
        std::memcpy(data.data(), &header, sizeof(MessageHeader));
        if (!body.empty()) {
            std::memcpy(data.data() + sizeof(MessageHeader), body.data(), body.size());
        }
        return data;
    }
    
    // 反序列化
    static Message deserialize(const std::vector<uint8_t>& data) {
        Message msg;
        if (data.size() >= sizeof(MessageHeader)) {
            std::memcpy(&msg.header, data.data(), sizeof(MessageHeader));
            if (msg.header.body_size > 0 && data.size() >= sizeof(MessageHeader) + msg.header.body_size) {
                msg.body.resize(msg.header.body_size);
                std::memcpy(msg.body.data(), data.data() + sizeof(MessageHeader), msg.header.body_size);
            }
        }
        return msg;
    }
};

// 连接选项
struct ConnectOptions {
    // 连接参数
    int connect_timeout = 5000;      // 连接超时 (ms)
    int heartbeat_interval = 30000;  // 心跳间隔 (ms)
    bool auto_reconnect = true;      // 自动重连
    int max_reconnect_attempts = 5;  // 最大重试次数
    
    // 安全选项
    std::string auth_token;          // 认证令牌
    bool enable_encryption = false;  // 启用加密
    std::string ssl_cert;            // SSL 证书
    
    // 性能选项
    size_t send_buffer_size = 65536; // 发送缓冲区
    size_t recv_buffer_size = 65536; // 接收缓冲区
    bool tcp_nodelay = true;         // 禁用 Nagle 算法
    bool keepalive = true;           // 启用保活
    
    // 高级选项
    std::string client_id;           // 客户端 ID
    std::map<std::string, std::string> metadata; // 元数据
};

// 服务器选项
struct ServerOptions {
    std::function<void(ConnectionID)> on_connect;
    std::function<void(ConnectionID, const Message&)> on_message;
    std::function<void(ConnectionID)> on_disconnect;
    size_t max_connections = 100;
    size_t send_buffer_size = 65536;
    size_t recv_buffer_size = 65536;
};

// 流选项
struct StreamOptions {
    bool reliable = true;
    size_t buffer_size = 65536;
    int timeout_ms = 5000;
};

// 服务信息
struct ServiceInfo {
    std::string name;
    std::string type;
    std::string address;
    std::string version;
    std::map<std::string, std::string> metadata;
    int64_t last_seen = 0;
};

// 统计信息
struct IPCStats {
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t errors = 0;
    uint64_t reconnects = 0;
    int64_t last_activity = 0;
};

// 池统计
struct PoolStats {
    size_t total_connections;
    size_t idle_connections;
    size_t active_connections;
};

// ============== IPC 通道接口 ==============

// IPC 通道接口
class IPCChannel {
public:
    virtual ~IPCChannel() = default;
    
    // 连接
    virtual bool connect(const std::string& address, const ConnectOptions& options = {}) = 0;
    
    // 监听
    virtual bool listen(const std::string& address, const ServerOptions& options = {}) = 0;
    
    // 发送消息
    virtual bool send(const Message& msg) = 0;
    
    // 接收消息
    virtual std::optional<Message> receive(int timeout_ms = -1) = 0;
    
    // 关闭
    virtual void close() = 0;
    
    // 检查连接是否有效
    virtual bool isValid() const = 0;
    
    // 获取统计信息
    virtual IPCStats getStats() const = 0;
};

// ============== Windows 命名管道实现 ==============

#ifdef _WIN32
class WindowsNamedPipe : public IPCChannel {
private:
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
    std::string pipe_name;
    bool is_server = false;
    IPCStats stats_;
    mutable std::mutex stats_mutex_;
    
public:
    WindowsNamedPipe() = default;
    ~WindowsNamedPipe() { close(); }
    
    bool connect(const std::string& address, const ConnectOptions& options = {}) override;
    
    bool listen(const std::string& address, const ServerOptions& options = {}) override;
    
    bool send(const Message& msg) override;
    
    std::optional<Message> receive(int timeout_ms) override;
    
    void close() override;
    
    bool isValid() const override {
        return pipe_handle != INVALID_HANDLE_VALUE;
    }
    
    IPCStats getStats() const override {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }
    
private:
    void updateStats(uint64_t sent, uint64_t recv, uint64_t err = 0);
};
#endif

// ============== Unix Socket 实现 ==============

#ifdef __unix__
class UnixSocket : public IPCChannel {
private:
    int sock_fd = -1;
    int client_fd = -1;
    sockaddr_un addr{};
    IPCStats stats_;
    mutable std::mutex stats_mutex_;
    
public:
    UnixSocket() = default;
    ~UnixSocket() { close(); }
    
    bool connect(const std::string& address, const ConnectOptions& options = {}) override;
    
    bool listen(const std::string& address, const ServerOptions& options = {}) override;
    
    bool send(const Message& msg) override;
    
    std::optional<Message> receive(int timeout_ms) override;
    
    void close() override;
    
    bool isValid() const override {
        return sock_fd != -1;
    }
    
    IPCStats getStats() const override {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }
    
private:
    void updateStats(uint64_t sent, uint64_t recv, uint64_t err = 0);
};
#endif

// ============== 连接池 ==============

class ConnectionPool {
private:
    std::string address;
    std::vector<std::unique_ptr<IPCChannel>> connections;
    std::vector<IPCChannel*> idle_connections;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    // 池配置
    size_t min_size = 2;
    size_t max_size = 10;
    size_t max_idle_time = 300000; // 5 分钟
    bool running = true;
    
public:
    ConnectionPool(const std::string& addr, const ConnectOptions& options = {});
    ~ConnectionPool();
    
    // 获取连接
    IPCChannel* acquire(int timeout_ms = 5000);
    
    // 释放连接
    void release(IPCChannel* conn);
    
    // 统计信息
    PoolStats get_stats() const;
    
    // 停止池
    void stop();

private:
    void initialize(const ConnectOptions& options);
    std::unique_ptr<IPCChannel> create_connection(const ConnectOptions& options);
    void cleanup_thread();
};

// ============== IPC 管理器 ==============

class IPCManager {
public:
    static IPCManager& getInstance() {
        static IPCManager instance;
        return instance;
    }
    
    // 连接管理
    ConnectionID connect(const std::string& address, const ConnectOptions& options = {});
    
    // 异步连接
    std::future<ConnectionID> connect_async(const std::string& address,
                                           std::function<void(ConnectionID, IPCErrorCode)> callback);
    
    // 监听管理
    ServerID listen(const std::string& address, const ServerOptions& options = {});
    
    // 消息发送
    std::future<Message> send_request(ConnectionID conn, const Message& request, int timeout_ms = 5000);
    
    // 发送通知
    void send_notification(ConnectionID conn, const Message& notification);
    
    // 批量操作
    std::vector<std::future<Message>> batch_send(ConnectionID conn, 
                                                 const std::vector<Message>& requests,
                                                 int timeout_ms = 5000);
    
    // 流式传输
    StreamID open_stream(ConnectionID conn, const StreamOptions& options = {});
    bool write_stream(StreamID stream, const uint8_t* data, size_t size);
    std::optional<std::vector<uint8_t>> read_stream(StreamID stream, size_t max_size, int timeout_ms);
    void close_stream(StreamID stream);
    
    // 订阅/发布
    SubscriptionID subscribe(ConnectionID conn, const std::string& topic,
                            std::function<void(const Message&)> callback);
    bool publish(ConnectionID conn, const std::string& topic, const Message& message);
    void unsubscribe(SubscriptionID sub);
    
    // 服务发现
    std::vector<ServiceInfo> discover_services(const std::string& service_type, int timeout_ms = 3000);
    
    // 连接池
    ConnectionPool* get_connection_pool(const std::string& address);
    
    // 统计信息
    IPCStats get_stats(ConnectionID conn);
    
    // 断开连接
    void disconnect(ConnectionID conn);
    
    // 关闭服务器
    void shutdown_server(ServerID server);
    
    // 调用外部函数（简化接口）
    Value callExternalFunction(const std::string& exePath, 
                               const std::string& funcName,
                               const std::vector<Value>& args);
    
    // 检查进程是否运行
    bool isProcessRunning(const std::string& exePath);
    
    // 启动进程
    bool startProcess(const std::string& exePath);

private:
    IPCManager() = default;
    
    struct ConnectionInfo {
        std::shared_ptr<IPCChannel> channel;
        ConnectOptions options;
        IPCStats stats;
        bool is_active = true;
    };
    
    struct ServerInfo {
        std::shared_ptr<IPCChannel> channel;
        ServerOptions options;
        std::thread accept_thread;
        bool is_running = true;
    };
    
    std::unordered_map<ConnectionID, ConnectionInfo> connections_;
    std::unordered_map<ServerID, ServerInfo> servers_;
    std::unordered_map<std::string, std::unique_ptr<ConnectionPool>> pools_;
    std::unordered_map<SubscriptionID, std::function<void(const Message&)>> subscriptions_;
    
    std::mutex mutex_;
    std::atomic<ConnectionID> next_conn_id{1};
    std::atomic<ServerID> next_server_id{1};
    std::atomic<StreamID> next_stream_id{1};
    std::atomic<SubscriptionID> next_sub_id{1};
};

// ============== ZCC 适配器（用于外部库） ==============

namespace adapter {

// 函数指针类型
using FunctionPtr = Value(*)(const std::vector<Value>&);

// 单例适配器
class Adapter {
private:
    static Adapter* instance;
    std::unordered_map<std::string, FunctionPtr> functions;
    std::shared_ptr<IPCChannel> ipc_channel;
    std::thread service_thread;
    bool running = false;
    std::mutex mutex_;
    std::atomic<uint32_t> sequence_id{1};
    
    Adapter() = default;
    
public:
    static Adapter& get() {
        if (!instance) {
            instance = new Adapter();
        }
        return *instance;
    }
    
    // 注册函数（简洁版）
    template<typename Func>
    void fn(const std::string& name, Func func) {
        std::lock_guard<std::mutex> lock(mutex_);
        functions[name] = [func](const std::vector<Value>& args) -> Value {
            return call_wrapper(func, args);
        };
    }
    
    // 注册函数（原始指针）
    void reg(const std::string& name, FunctionPtr func);
    
    // 启动服务
    void serve(const std::string& address);
    
    // 运行（别名）
    void run(const std::string& address = "zcc") {
        serve(address);
    }
    
    // 停止服务
    void stop();
    
    // 检查是否运行
    bool isRunning() const { return running; }

private:
    void service_loop();
    void handle_message(const Message& msg);
    Message create_response(const Message& request, const Value& result);
    Message create_error_response(const Message& request, const std::string& error);
    
    template<typename Func>
    Value call_wrapper(Func func, const std::vector<Value>& args) {
        return func(args);
    }
};

// 全局注册宏
#define ZCC_REGISTER_FUNC(name) \
    static struct ZCCFuncRegistrar_##name { \
        ZCCFuncRegistrar_##name() { \
            Adapter::get().fn(#name, name); \
        } \
    } zcc_registrar_##name;

} // namespace adapter

} // namespace zcc

#endif // ZCC_IPC_HPP
