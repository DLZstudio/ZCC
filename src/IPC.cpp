#include "IPC.hpp"
#include <cstring>
#include <chrono>

namespace zcc {

// ==================== Windows Named Pipe 实现 ====================

#ifdef _WIN32

void WindowsNamedPipe::updateStats(uint64_t sent, uint64_t recv, uint64_t err) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent += sent > 0 ? 1 : 0;
    stats_.messages_received += recv > 0 ? 1 : 0;
    stats_.bytes_sent += sent;
    stats_.bytes_received += recv;
    stats_.errors += err;
    stats_.last_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool WindowsNamedPipe::connect(const std::string& address, const ConnectOptions& options) {
    if (pipe_handle != INVALID_HANDLE_VALUE) {
        close();
    }
    
    pipe_name = R"(\\.\pipe\)" + address;
    
    // 尝试连接
    int attempts = 0;
    int max_attempts = options.max_reconnect_attempts;
    
    while (attempts < max_attempts) {
        pipe_handle = CreateFileA(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (pipe_handle != INVALID_HANDLE_VALUE) {
            is_server = false;
            return true;
        }
        
        DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY) {
            updateStats(0, 0, 1);
            return false;
        }
        
        // 等待管道可用
        if (!WaitNamedPipeA(pipe_name.c_str(), options.connect_timeout)) {
            attempts++;
            if (attempts >= max_attempts) {
                updateStats(0, 0, 1);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
    updateStats(0, 0, 1);
    return false;
}

bool WindowsNamedPipe::listen(const std::string& address, const ServerOptions& options) {
    pipe_name = R"(\\.\pipe\)" + address;
    
    pipe_handle = CreateNamedPipeA(
        pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        static_cast<DWORD>(options.send_buffer_size),
        static_cast<DWORD>(options.recv_buffer_size),
        0,
        nullptr
    );
    
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    is_server = true;
    
    // 等待客户端连接
    if (!ConnectNamedPipe(pipe_handle, nullptr)) {
        DWORD error = GetLastError();
        if (error != ERROR_PIPE_CONNECTED) {
            close();
            return false;
        }
    }
    
    return true;
}

bool WindowsNamedPipe::send(const Message& msg) {
    if (!isValid()) {
        return false;
    }
    
    DWORD bytes_written = 0;
    
    // 序列化消息
    auto data = msg.serialize();
    
    // 发送数据
    if (!WriteFile(pipe_handle, data.data(), static_cast<DWORD>(data.size()), &bytes_written, nullptr)) {
        updateStats(0, 0, 1);
        return false;
    }
    
    updateStats(1, 0, 0);
    return true;
}

std::optional<Message> WindowsNamedPipe::receive(int timeout_ms) {
    if (!isValid()) {
        return std::nullopt;
    }
    
    MessageHeader header;
    DWORD bytes_read = 0;
    
    // 设置读取超时
    if (timeout_ms >= 0) {
        DWORD timeout = static_cast<DWORD>(timeout_ms);
        SetNamedPipeHandleState(pipe_handle, &timeout, nullptr, nullptr);
    }
    
    // 读取消息头
    if (!ReadFile(pipe_handle, &header, sizeof(header), &bytes_read, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            close();
        }
        updateStats(0, 0, 1);
        return std::nullopt;
    }
    
    // 读取消息体
    std::vector<uint8_t> body(header.body_size);
    if (header.body_size > 0) {
        if (!ReadFile(pipe_handle, body.data(), header.body_size, &bytes_read, nullptr)) {
            updateStats(0, 0, 1);
            return std::nullopt;
        }
    }
    
    updateStats(0, 1, 0);
    
    Message msg;
    msg.header = header;
    msg.body = std::move(body);
    return msg;
}

void WindowsNamedPipe::close() {
    if (pipe_handle != INVALID_HANDLE_VALUE) {
        if (is_server) {
            DisconnectNamedPipe(pipe_handle);
        }
        CloseHandle(pipe_handle);
        pipe_handle = INVALID_HANDLE_VALUE;
    }
    is_server = false;
}

#endif

// ==================== Unix Socket 实现 ====================

#ifdef __unix__

void UnixSocket::updateStats(uint64_t sent, uint64_t recv, uint64_t err) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent += sent > 0 ? 1 : 0;
    stats_.messages_received += recv > 0 ? 1 : 0;
    stats_.bytes_sent += sent;
    stats_.bytes_received += recv;
    stats_.errors += err;
    stats_.last_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool UnixSocket::connect(const std::string& address, const ConnectOptions& options) {
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        return false;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Unix socket 路径长度限制
    if (address.size() > sizeof(addr.sun_path) - 1) {
        close();
        return false;
    }
    
    strncpy(addr.sun_path, address.c_str(), sizeof(addr.sun_path) - 1);
    
    // 设置连接超时
    if (options.connect_timeout > 0) {
        struct timeval tv;
        tv.tv_sec = options.connect_timeout / 1000;
        tv.tv_usec = (options.connect_timeout % 1000) * 1000;
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    
    // 尝试连接
    int retries = options.max_reconnect_attempts;
    while (retries-- > 0) {
        if (::connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            return true;
        }
        
        if (errno != ENOENT && errno != ECONNREFUSED) {
            break;
        }
        
        // 等待并重试
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    updateStats(0, 0, 1);
    close();
    return false;
}

bool UnixSocket::listen(const std::string& address, const ServerOptions& options) {
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        return false;
    }
    
    // 删除已存在的 socket 文件
    unlink(address.c_str());
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, address.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close();
        return false;
    }
    
    if (::listen(sock_fd, static_cast<int>(options.max_connections)) < 0) {
        close();
        return false;
    }
    
    return true;
}

bool UnixSocket::send(const Message& msg) {
    int fd = client_fd != -1 ? client_fd : sock_fd;
    if (fd < 0) {
        return false;
    }
    
    // 序列化消息
    auto data = msg.serialize();
    
    // 发送数据
    ssize_t total_sent = 0;
    while (total_sent < static_cast<ssize_t>(data.size())) {
        ssize_t n = write(fd, data.data() + total_sent, data.size() - total_sent);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            updateStats(0, 0, 1);
            return false;
        }
        total_sent += n;
    }
    
    updateStats(1, 0, 0);
    return true;
}

std::optional<Message> UnixSocket::receive(int timeout_ms) {
    int fd = client_fd != -1 ? client_fd : sock_fd;
    if (fd < 0) {
        return std::nullopt;
    }
    
    // 设置超时
    if (timeout_ms >= 0) {
        timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    MessageHeader header;
    ssize_t n = read(fd, &header, sizeof(header));
    if (n != sizeof(header)) {
        if (n == 0 || errno == ECONNRESET) {
            close();
        }
        updateStats(0, 0, 1);
        return std::nullopt;
    }
    
    // 读取消息体
    std::vector<uint8_t> body(header.body_size);
    if (header.body_size > 0) {
        n = read(fd, body.data(), header.body_size);
        if (n != static_cast<ssize_t>(header.body_size)) {
            updateStats(0, 0, 1);
            return std::nullopt;
        }
    }
    
    updateStats(0, 1, 0);
    
    Message msg;
    msg.header = header;
    msg.body = std::move(body);
    return msg;
}

void UnixSocket::close() {
    if (client_fd != -1) {
        ::close(client_fd);
        client_fd = -1;
    }
    if (sock_fd != -1) {
        ::close(sock_fd);
        sock_fd = -1;
    }
}

#endif

// ==================== ConnectionPool 实现 ====================

ConnectionPool::ConnectionPool(const std::string& addr, const ConnectOptions& options) 
    : address(addr) {
    initialize(options);
    
    // 启动清理线程
    std::thread(&ConnectionPool::cleanup_thread, this).detach();
}

ConnectionPool::~ConnectionPool() {
    stop();
}

void ConnectionPool::initialize(const ConnectOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t i = 0; i < min_size; ++i) {
        auto conn = create_connection(options);
        if (conn) {
            connections.push_back(std::move(conn));
            idle_connections.push_back(connections.back().get());
        }
    }
}

std::unique_ptr<IPCChannel> ConnectionPool::create_connection(const ConnectOptions& options) {
#ifdef _WIN32
    auto conn = std::make_unique<WindowsNamedPipe>();
#else
    auto conn = std::make_unique<UnixSocket>();
#endif
    
    if (conn->connect(address, options)) {
        return conn;
    }
    return nullptr;
}

IPCChannel* ConnectionPool::acquire(int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    while (idle_connections.empty()) {
        // 尝试创建新连接
        if (connections.size() < max_size) {
            // 需要解锁创建连接
            lock.unlock();
            
            ConnectOptions options;
            auto conn = create_connection(options);
            
            lock.lock();
            if (conn) {
                connections.push_back(std::move(conn));
                return connections.back().get();
            }
        }
        
        // 等待有空闲连接
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            return nullptr;
        }
    }
    
    IPCChannel* conn = idle_connections.back();
    idle_connections.pop_back();
    return conn;
}

void ConnectionPool::release(IPCChannel* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查连接是否还在池中
    bool found = false;
    for (auto& c : connections) {
        if (c.get() == conn) {
            found = true;
            break;
        }
    }
    
    if (found) {
        idle_connections.push_back(conn);
        cv_.notify_one();
    }
}

PoolStats ConnectionPool::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    PoolStats stats;
    stats.total_connections = connections.size();
    stats.idle_connections = idle_connections.size();
    stats.active_connections = stats.total_connections - stats.idle_connections;
    return stats;
}

void ConnectionPool::stop() {
    running = false;
    cv_.notify_all();
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& conn : connections) {
        if (conn) {
            conn->close();
        }
    }
    connections.clear();
    idle_connections.clear();
}

void ConnectionPool::cleanup_thread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(max_idle_time / 2));
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 清理超时的空闲连接
        auto now = std::chrono::steady_clock::now();
        auto it = idle_connections.begin();
        while (it != idle_connections.end()) {
            // 简单实现：如果连接数超过最小值，移除
            if (connections.size() > min_size && idle_connections.size() > min_size) {
                it = idle_connections.erase(it);
                
                // 从 connections 中移除
                auto conn_it = std::find_if(connections.begin(), connections.end(),
                    [ptr = *it](const auto& c) { return c.get() == ptr; });
                if (conn_it != connections.end()) {
                    connections.erase(conn_it);
                }
            } else {
                ++it;
            }
        }
    }
}

// ==================== IPCManager 实现 ====================

IPCManager::ConnectionID IPCManager::connect(const std::string& address, const ConnectOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConnectionID id = next_conn_id++;
    
#ifdef _WIN32
    auto channel = std::make_shared<WindowsNamedPipe>();
#else
    auto channel = std::make_shared<UnixSocket>();
#endif
    
    if (!channel->connect(address, options)) {
        return 0;  // 连接失败
    }
    
    connections_[id] = ConnectionInfo{
        .channel = channel,
        .options = options,
        .stats = IPCStats(),
        .is_active = true
    };
    
    return id;
}

std::future<ConnectionID> IPCManager::connect_async(
    const std::string& address,
    std::function<void(ConnectionID, IPCErrorCode)> callback) {
    
    auto promise = std::make_shared<std::promise<ConnectionID>>();
    
    std::thread([=]() {
        ConnectOptions options;
        auto id = connect(address, options);
        
        if (id != 0) {
            promise->set_value(id);
            if (callback) {
                callback(id, IPCErrorCode::SUCCESS);
            }
        } else {
            promise->set_value(0);
            if (callback) {
                callback(0, IPCErrorCode::CONNECTION_FAILED);
            }
        }
    }).detach();
    
    return promise->get_future();
}

IPCManager::ServerID IPCManager::listen(const std::string& address, const ServerOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ServerID id = next_server_id++;
    
#ifdef _WIN32
    auto channel = std::make_shared<WindowsNamedPipe>();
#else
    auto channel = std::make_shared<UnixSocket>();
#endif
    
    if (!channel->listen(address, options)) {
        return 0;
    }
    
    auto& info = servers_[id] = ServerInfo{
        .channel = channel,
        .options = options,
        .accept_thread = std::thread(),
        .is_running = true
    };
    
    // 启动接受连接线程
    info.accept_thread = std::thread([this, id, &info]() {
        while (info.is_running) {
            // 等待客户端连接（简化实现）
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    return id;
}

std::future<Message> IPCManager::send_request(
    ConnectionID conn, const Message& request, int timeout_ms) {
    
    auto promise = std::make_shared<std::promise<Message>>();
    
    std::thread([=]() {
        auto it = connections_.find(conn);
        if (it == connections_.end() || !it->second.is_active) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("连接不存在")));
            return;
        }
        
        auto& channel = it->second.channel;
        
        // 发送请求
        if (!channel->send(request)) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("发送失败")));
            return;
        }
        
        // 接收响应
        auto response = channel->receive(timeout_ms);
        if (response) {
            promise->set_value(*response);
        } else {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("接收超时")));
        }
    }).detach();
    
    return promise->get_future();
}

void IPCManager::send_notification(ConnectionID conn, const Message& notification) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(conn);
    if (it != connections_.end() && it->second.is_active) {
        it->second.channel->send(notification);
    }
}

std::vector<std::future<Message>> IPCManager::batch_send(
    ConnectionID conn,
    const std::vector<Message>& requests,
    int timeout_ms) {
    
    std::vector<std::future<Message>> futures;
    
    for (const auto& req : requests) {
        futures.push_back(send_request(conn, req, timeout_ms));
    }
    
    return futures;
}

IPCManager::StreamID IPCManager::open_stream(ConnectionID conn, const StreamOptions& options) {
    StreamID id = next_stream_id++;
    // 简化实现：实际应该创建流对象
    return id;
}

bool IPCManager::write_stream(StreamID stream, const uint8_t* data, size_t size) {
    // 简化实现
    return true;
}

std::optional<std::vector<uint8_t>> IPCManager::read_stream(
    StreamID stream, size_t max_size, int timeout_ms) {
    // 简化实现
    return std::nullopt;
}

void IPCManager::close_stream(StreamID stream) {
    // 简化实现
}

IPCManager::SubscriptionID IPCManager::subscribe(
    ConnectionID conn,
    const std::string& topic,
    std::function<void(const Message&)> callback) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    SubscriptionID id = next_sub_id++;
    subscriptions_[id] = callback;
    
    // 发送订阅请求
    Message sub_msg;
    sub_msg.header.message_type = static_cast<uint32_t>(MessageType::SUBSCRIBE);
    sub_msg.topic = topic;
    
    send_notification(conn, sub_msg);
    
    return id;
}

bool IPCManager::publish(ConnectionID conn, const std::string& topic, const Message& message) {
    Message pub_msg;
    pub_msg.header.message_type = static_cast<uint32_t>(MessageType::PUBLISH);
    pub_msg.topic = topic;
    pub_msg.body = message.body;
    
    send_notification(conn, pub_msg);
    return true;
}

void IPCManager::unsubscribe(SubscriptionID sub) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.erase(sub);
}

std::vector<ServiceInfo> IPCManager::discover_services(
    const std::string& service_type, int timeout_ms) {
    // 简化实现：返回空列表
    return {};
}

ConnectionPool* IPCManager::get_connection_pool(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(address);
    if (it == pools_.end()) {
        pools_[address] = std::make_unique<ConnectionPool>(address);
    }
    
    return pools_[address].get();
}

IPCStats IPCManager::get_stats(ConnectionID conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(conn);
    if (it != connections_.end()) {
        return it->second.channel->getStats();
    }
    
    return IPCStats();
}

void IPCManager::disconnect(ConnectionID conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(conn);
    if (it != connections_.end()) {
        it->second.channel->close();
        it->second.is_active = false;
        connections_.erase(it);
    }
}

void IPCManager::shutdown_server(ServerID server) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = servers_.find(server);
    if (it != servers_.end()) {
        it->second.is_running = false;
        if (it->second.accept_thread.joinable()) {
            it->second.accept_thread.join();
        }
        it->second.channel->close();
        servers_.erase(it);
    }
}

// ==================== zcc_adapter::Adapter 实现 ====================

namespace adapter {

Adapter* Adapter::instance = nullptr;

void Adapter::reg(const std::string& name, FunctionPtr func) {
    std::lock_guard<std::mutex> lock(mutex_);
    functions[name] = func;
}

void Adapter::serve(const std::string& address) {
    if (running) {
        return;
    }
    
#ifdef _WIN32
    ipc_channel = std::make_shared<WindowsNamedPipe>();
#else
    ipc_channel = std::make_shared<UnixSocket>();
#endif
    
    ServerOptions options;
    if (!ipc_channel->listen(address, options)) {
        throw std::runtime_error("无法启动 IPC 服务：" + address);
    }
    
    running = true;
    service_thread = std::thread(&Adapter::service_loop, this);
}

void Adapter::stop() {
    running = false;
    if (service_thread.joinable()) {
        service_thread.join();
    }
    if (ipc_channel) {
        ipc_channel->close();
    }
}

void Adapter::service_loop() {
    while (running) {
        auto msg = ipc_channel->receive(1000);
        if (!msg) {
            continue;
        }
        
        handle_message(*msg);
    }
}

void Adapter::handle_message(const Message& msg) {
    if (msg.header.message_type != static_cast<uint32_t>(MessageType::REQUEST)) {
        return;
    }
    
    // 查找函数
    std::string func_name = msg.method;
    auto it = functions.find(func_name);
    
    if (it == functions.end()) {
        // 函数未找到
        auto response = create_error_response(msg, "函数未找到：" + func_name);
        ipc_channel->send(response);
        return;
    }
    
    try {
        // 调用函数（简化实现：无参数）
        std::vector<Value> args;
        Value result = it->second(args);
        
        // 发送响应
        auto response = create_response(msg, result);
        ipc_channel->send(response);
        
    } catch (const std::exception& e) {
        auto response = create_error_response(msg, e.what());
        ipc_channel->send(response);
    }
}

Message Adapter::create_response(const Message& request, const Value& result) {
    Message response;
    response.header.message_type = static_cast<uint32_t>(MessageType::RESPONSE);
    response.header.sequence_id = request.header.sequence_id;
    // 序列化 result 到 response.result
    return response;
}

Message Adapter::create_error_response(const Message& request, const std::string& error) {
    Message response;
    response.header.message_type = static_cast<uint32_t>(MessageType::IPC_ERROR);
    response.header.sequence_id = request.header.sequence_id;
    response.error_message = error;
    return response;
}

} // namespace adapter

} // namespace zcc
