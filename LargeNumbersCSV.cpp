// LargeNumberCSV.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>

#define _WIN32_WINNT 0x0601

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void WriteCVS(std::string str);

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Sends a WebSocket message and prints the response
class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string text_;
    std::vector<std::string> linesToCalc_;
    std::vector<std::string>::iterator linesToCalc_it_;
public:
    // Resolver and socket require an io_context
    explicit
        session(net::io_context& ioc)
        : resolver_(net::make_strand(ioc))
        , ws_(net::make_strand(ioc))
    {
    }

    // Start the asynchronous operation
    void
        run(
            char const* host,
            char const* port,
            std::vector<std::string> strVec)
    {
        // Save these for later
        host_ = host;
        text_;
        linesToCalc_ = strVec;
        linesToCalc_it_ = linesToCalc_.begin();

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &session::on_resolve,
                shared_from_this()));
    }

    void
        on_resolve(
            beast::error_code ec,
            tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, "resolve");

        // Set the timeout for the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(ws_).async_connect(
            results,
            beast::bind_front_handler(
                &session::on_connect,
                shared_from_this()));
    }

    void
        on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
        if (ec)
            return fail(ec, "connect");

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(ws_).expires_never();

        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-async");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        host_ += ':' + std::to_string(ep.port());

        // Perform the websocket handshake
        ws_.async_handshake(host_, "/",
            beast::bind_front_handler(
                &session::on_handshake,
                shared_from_this()));
    }

    void
        on_handshake(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "handshake");

        if (linesToCalc_it_ == linesToCalc_.end()) return;
        text_ = *linesToCalc_it_;
        linesToCalc_it_++;

        // Send the message
        ws_.async_write(
            net::buffer(text_),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void
        on_write(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "read");

        // Echo the message
        ws_.text(ws_.got_text());
        std::string data = boost::beast::buffers_to_string(buffer_.data());
        std::cout << "Received result: " << data << "\n";

        // Clear the buffer
        buffer_.consume(buffer_.size());

        WriteCVS(data);

        if (linesToCalc_it_ != linesToCalc_.end()) {
            text_ = *linesToCalc_it_;
            linesToCalc_it_++;
            if (text_.size() == 0)return;

            // Send the message
            ws_.async_write(
                net::buffer(text_),
                beast::bind_front_handler(
                    &session::on_write,
                    shared_from_this()));
        }

                /*// Close the WebSocket connection
                ws_.async_close(websocket::close_code::normal,
                    beast::bind_front_handler(
                        &session::on_close,
                        shared_from_this()));*/
    }

    void
        on_close(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "close");

        // If we get here then the connection is closed gracefully

        // The make_printable() function helps print a ConstBufferSequence
        std::cout << beast::make_printable(buffer_.data()) << std::endl;
    }
};

//----------------------------------------------------------------------------

std::vector<std::string> ReadCVS() {

    std::fstream fin;
    fin.open("..\\Data\\sampleData.csv", std::ios::in);
    std::vector<std::string> linestrVec;
    std::string linestr;

    std::cout << "Reading CSV file: "<< "..\\Data\\sampleData.csv" << "\n";

    while (std::getline(fin, linestr)) {

        std::cout << "Sending calculation request to the server: " << linestr << std::endl;

        linestrVec.push_back(linestr);
        /*
        std::istringstream input;
        input.str(linestr);
        while (std::getline(input, currElement, ',')) {

            std::cout << "Element found: " << currElement << std::endl;

        }*/

    }

    return linestrVec;
}

void WriteCVS(std::string str) {

    std::fstream fout;
    fout.open("..\\Data\\sampleOut.csv", std::ios_base::app);

    if (fout.is_open())
    {
        fout.write(str.c_str(), str.size());
        fout << "\n";
    }
    else
    {
        std::cout << "Error writing to file\n";
    }
    fout.close();

}

int main(int argc, char** argv)
{

    std::cout << "argc: " << argc << std::endl;
    std::cout << "0: " << argv[0] << std::endl;
    std::cout << "1: " << argv[1] << std::endl;
    std::cout << "2: " << argv[2] << std::endl;

    std::vector<std::string> linesToCalculate;
    linesToCalculate = ReadCVS();

    // Check command line arguments.
    if (argc != 3)
    {
        std::cerr <<
            "Usage: LargeNumbersCSV.exe <host> <port> \n" <<
            "Example:\n" <<
            "    LargeNumbersCSV.exe 127.0.0.1 8080\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    std::make_shared<session>(ioc)->run(host, port, linesToCalculate);

    // Run the I/O service. The call will return when
    // the socket is closed.
    ioc.run();

    std::system("PAUSE");

    return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
