#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>
#include "WebhookMgr.h"
#include "Chat.h"
#include <csignal>

WebhookMgr* WebhookMgr::instance()
{
    static WebhookMgr instance;
    return &instance;
}

WebhookMgr::~WebhookMgr()
{
    Stop(); // Ensure the worker thread is stopped
}

void WebhookMgr::ScheduleMessage(const std::string& rawMessage)
{
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _messageQueue.push(rawMessage);
    }
    _condition.notify_one(); // Notify the worker thread that a message is available
}

void WebhookMgr::Start()
{
    _stopWorker = false;
    _workerThread = std::thread(&WebhookMgr::ProcessMessages, this);
}

void WebhookMgr::Stop()
{
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _stopWorker = true; // Signal the thread to stop
    }
    _condition.notify_all(); // Wake up the thread if it’s waiting

    if (_workerThread.joinable())
    {
        _workerThread.join(); // Wait for the thread to finish
    }
}

void WebhookMgr::ProcessMessages()
{
    while (true) {
        std::string message;

        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _condition.wait(lock, [this]
                {
                return !_messageQueue.empty() || _stopWorker;
                });

            // Exit immediately if stopWorker is set
            if (_stopWorker)
            {
                // Clear the queue
                std::queue<std::string> emptyQueue;
                std::swap(_messageQueue, emptyQueue);
                break;
            }

            // Retrieve the next message
            message = _messageQueue.front();
            _messageQueue.pop();
        }

        // Send the message (rate-limited)
        SendHttpPost(message);

        std::this_thread::sleep_for(std::chrono::milliseconds(600)); // Break for rate limit. Currently, 5 requests per 2 seconds
    }

    LOG_INFO("server.worldserver", "Webhook processor closed.");
}

void WebhookMgr::SendHttpPost(const std::string& rawMessage)
{
    try {
        if (_webhookUrl.empty())
        {
            LOG_ERROR("server.loading", "The webhook url is empty.");
            return;
        }

        // 1. Parse the global _webhookUrl
        // Expected: https://domain.com/api/path...
        std::string host, apiPath;
        std::string urlCopy = _webhookUrl;

        // Remove "https://" or "http://" prefix
        size_t protoEnd = urlCopy.find("://");
        if (protoEnd != std::string::npos)
            urlCopy.erase(0, protoEnd + 3);

        // Split into Host and Path
        size_t pathStart = urlCopy.find("/");
        if (pathStart == std::string::npos) {
            host = urlCopy;
            apiPath = "/";
        } else {
            host = urlCopy.substr(0, pathStart);
            apiPath = urlCopy.substr(pathStart);
        }

        // 2. Setup Network Contexts
        boost::asio::io_context io_context;
        boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23_client);
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(io_context, ssl_context);

        // 3. Connect & Handshake (Targeting HTTPS Port 443)
        auto const endpoints = resolver.resolve(host, "443");
        boost::asio::connect(stream.next_layer(), endpoints);
        stream.handshake(boost::asio::ssl::stream_base::client);

        // 4. Construct HTTP POST Request
        std::string request =
            "POST " + apiPath + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "Content-Type: application/json\r\n" +
            "Content-Length: " + std::to_string(jsonPayload.size()) + "\r\n" +
            "Connection: close\r\n\r\n" +
            jsonPayload;

        // 5. Send and Read Response Status
        boost::asio::write(stream, boost::asio::buffer(request));

        boost::asio::streambuf response;
        boost::asio::read_until(stream, response, "\r\n");
        std::istream response_stream(&response);
        
        std::string http_version;
        unsigned int status_code;
        response_stream >> http_version >> status_code;

        if (status_code < 200 || status_code >= 300)
        {
            std::ostringstream ss;
            ss << "POST failed. Host: " << host << " Status: " << status_code;
            LOG_ERROR("server.worldserver", ss.str());
        }
    }
    catch (const std::exception& e)
    {
        std::ostringstream ss;
        ss << "Network Error: " << e.what();
        LOG_ERROR("server.worldserver", ss.str());
    }
}

void WebhookMgr::SetWebhookUrl(std::string& url)
{
    _webhookUrl = url;
}

