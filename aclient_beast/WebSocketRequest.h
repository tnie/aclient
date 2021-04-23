#pragma once
#include "AClient.h"

namespace websocket = beast::websocket;

class WebSocketRequest final: public std::enable_shared_from_this<WebSocketRequest>
{
    friend class HTTPClient;
    WebSocketRequest(asio::io_context& ioc, unsigned int id);
    // ���е��첽�ص�ȫ������ this ָ�롣�����ṩ��������&�ƶ���������ͣ��޷���Ϊ vector ��Ԫ��
    WebSocketRequest(const WebSocketRequest&) = delete;
    WebSocketRequest& operator==(const WebSocketRequest&) = delete;
    WebSocketRequest(WebSocketRequest&&) = delete;
    WebSocketRequest& operator==(WebSocketRequest&&) = delete;
public:
    using Callback = std::function<void(const WebSocketRequest&, const beast::flat_buffer&, const std::error_code&)>;
    ~WebSocketRequest();
    /// <param name="port">Ϊ��ʱ����Э������ʹ��Ĭ�϶˿ڡ�http 80 https 443</param>
    void set_task(std::string host, bool wss = true, std::string target = "", unsigned port = 0);
    void send(const std::string& msg);
    void close();
    void execute();
    void cancel() {};
private:
    std::string host_;
    std::string target_;
    unsigned port_;
    beast::flat_buffer buffer_;
    Callback handler_;
    unsigned int uuid_;
    //
    asio::io_context& ioc_;
    asio::ip::tcp::resolver resolver_;
    // �� ssocket_(+ctx_) ��ѡһ
    std::shared_ptr<websocket::stream<beast::tcp_stream>> ws_;
    std::shared_ptr<asio::ssl::context> ctx_;    // TODO reference?
    std::shared_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ssocket_;
    bool cancel_ = false;   // �ͻ�������ȡ��
    asio::steady_timer timer_;
};

