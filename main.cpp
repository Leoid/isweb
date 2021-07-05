#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>

using namespace std;
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using boost::asio::ip::tcp;

#define NUM_THREADS 100
std::mutex req_mutex;

struct Domain
{
    string host;
    string port;
};

class client
{
public:
    client(boost::asio::io_service &io_service,
           const std::string &server, const std::string &path)
        : resolver_(io_service),
          socket_(io_service)
    {
        server_ = server;
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.

        std::ostream request_stream(&request_);
        request_stream << "HEAD " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

        // Start an asynchronous resolve to translate the server and service names
        // into a list of endpoints.
        // cout << "Checking: " << server << endl;
        // tcp::resolver::query query(server, "http");
        tcp::resolver::query query(server, "80", boost::asio::ip::resolver_query_base::numeric_service);

        resolver_.async_resolve(query,
                                boost::bind(&client::handle_resolve, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::iterator));
    }
  
private:
    void handle_resolve(const boost::system::error_code &err,
                        tcp::resolver::iterator endpoint_iterator)
    {
        if (!err)
        {
            // Attempt a connection to the first endpoint in the list. Each endpoint
            // will be tried until we successfully establish a connection.
            tcp::endpoint endpoint = *endpoint_iterator;

            socket_.async_connect(endpoint,
                                  boost::bind(&client::handle_connect, this,
                                              boost::asio::placeholders::error, ++endpoint_iterator));
        }
        else
        {
            // std::cout << "Error4: " << err.message() << "\n";
            
        }
    }

    void handle_connect(const boost::system::error_code &err,
                        tcp::resolver::iterator endpoint_iterator)
    {

        if (!err)
        {
            // The connection was successful. Send the request.
            boost::asio::async_write(socket_, request_,
                                     boost::bind(&client::handle_write_request, this,
                                                 boost::asio::placeholders::error));
        }
        // Check if the connect operation failed before the deadline expired.
        // else if (err)
        // {
        //     // std::cout << "Connect error: " << err.message() << "\n";

        //     // We need to close the socket used in the previous connection attempt
        //     // before starting a new one.
        //     socket_.close();

        //     // Try the next available endpoint.
        //     // handle_resolve(err, ++endpoint_iterator);
        // }
        else if (endpoint_iterator != tcp::resolver::iterator())
        {
            // The connection failed. Try the next endpoint in the list.
            socket_.close();
            tcp::endpoint endpoint = *endpoint_iterator;

            socket_.async_connect(endpoint,
                                  boost::bind(&client::handle_connect, this,
                                              boost::asio::placeholders::error, ++endpoint_iterator));
        }
        else
        {
            // Check HTTPS
            try
            {

                boost::asio::io_context ioc;
                // Look up the domain name
                // These objects perform our I/O
                tcp::resolver resolver(ioc);
                beast::tcp_stream stream(ioc);
                auto const results = resolver.resolve(this->server_, "443");

                // Make the connection on the IP address we get from a lookup
                stream.connect(results);

                // Set the logical operation timer to 100 milliseconds.
                stream.expires_after(std::chrono::milliseconds(100));
                //   std::cout << "Error3: " << err.message() << "\n";
                // Gracefully close the socket
                beast::error_code ec;
                stream.socket().shutdown(tcp::socket::shutdown_both, ec);
                if (ec && ec != beast::errc::not_connected)
                {
                    throw beast::system_error{ec};
                }
                else
                {
                    // Set up an HTTP GET request message
                    auto const target = "/";
                    int version = 11;
                    http::request<http::string_body> req{http::verb::head, target, version};
                    req.set(http::field::host, this->server_);
                    // req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
                    // Send the HTTP request to the remote host
                    http::write(stream, req);

                    // // NULL buffer is used for reading and must be persisted
                    beast::flat_buffer buffer;

                    // // Declare a container to hold the response
                    http::response<http::dynamic_body> res;

                    // // Receive the HTTP response
                    http::read(stream, buffer, res);

                    // // Write the message to standard out
                    std::cout << res.has_content_length() << std::endl;
                    // cout << res.has_content_length() << endl;
                    http::status status_code = res.result();

                    // cout << "[+] https://" << this->server_ << endl;
                    req_mutex.lock();
                    printf("[+] https://%s\n", this->server_.c_str());
                    req_mutex.unlock();
                }

                // If we get here then the connection is closed gracefully
                stream.close();
            }
            catch (std::exception const &e)
            {
                // printf("[-] \t%s - %s\n", this->server_.c_str(), e.what());
            }
        }
    }

    void handle_write_request(const boost::system::error_code &err)
    {
        if (!err)
        {
            // Read the response status line.
            boost::asio::async_read_until(socket_, response_, "\r\n",
                                          boost::bind(&client::handle_read_status_line, this,
                                                      boost::asio::placeholders::error));
        }
        // else
        // {
        //   std::cout << "Error2: " << err.message() << "\n";
        // }
    }

    void handle_read_status_line(const boost::system::error_code &err)
    {
        if (!err)
        {
            // Check that response is OK.
            std::istream response_stream(&response_);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            req_mutex.lock();
            cout << "[+] http://" << this->server_ << " - Status Code: " << status_code << endl;
            req_mutex.unlock();
            //   cout<< "status code: " << status_code << endl;
        }
        // else
        // {
        //   std::cout << "Error1: " << err << endl;
        // }
    }

    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;
    string server_;


};

vector<Domain> split_vec(vector<Domain> domains, size_t start, size_t size, size_t vec_size)
{
    vector<Domain> temp;
    for (size_t i = start; i <= size; i++)
    {
        if (i < vec_size)
        {
            // cout << "start: " << start << " " << domains[i].host << endl;
            temp.push_back(domains[i]);
        }
    }
    // cout << "temp size: " << temp.size() << endl;
    return temp;
}
std::atomic <unsigned int> g_thread(0);
void *is_web(void *d1)
{
    vector<Domain> *domains_vector = (vector<Domain> *)d1;
    // printf("vec size: %d\n", domains_vector->size());
    for (auto d : *domains_vector)
    {
        
        // cout << "Printing Start Host: " << d.host << endl;
        // std::this_thread::sleep_for (std::chrono::seconds(3));
        // cout << "Printing End Host: " << d.host << endl;

        string sub_host = d.host;
        string port = d.port;

        // cout << "test: " << sub_host << endl;
        auto const target = "/";
        int version = 11;
        try
        {
            // The io_context is required for all I/O
            boost::asio::io_service io_service;
            client c(
                io_service,
                sub_host,
                target);

            // Run
            // req_mutex.lock();
            io_service.run();
            // req_mutex.unlock();
            
        }
        catch (std::exception const &e)
        {
            // printf("[-] \t%s - %s\n", sub_host.c_str(), e.what());
        }
        // pthread_exit(NULL);
        g_thread++;
    }
    
}

int main(int argc, char **argv)
{
    // Get the host/domain list
    ofstream host_file;
    // Open the File
    std::ifstream in(argv[1]);
    string sub_host;
    vector<Domain> domains;
    pthread_t threads[NUM_THREADS];
    int rc;
    // Read the next line from File untill it reaches the end.
    while (getline(in, sub_host))
    {
        // Line contains string of length > 0 then save it in vector
        if (sub_host.size() > 0)
        {
            struct Domain d1;
            d1.host = sub_host;
            d1.port = "443";
            domains.push_back(d1);
        }
    }
    //Close The File
    in.close();

    // Chunk the domains/hosts list
    int chunk_size = domains.size() / NUM_THREADS;
    int current_index = 0;
    int chunk_end = chunk_size;

    vector <vector<Domain>> m_vec;

    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        // printf("%d: %d - %d\n",i,current_index,chunk_end-1);
        current_index += chunk_size;
        chunk_end += chunk_size;
        vector<Domain> temp;
        temp = split_vec(domains, current_index, chunk_end - 1, domains.size());
        m_vec.push_back(temp);
       
        // printf("joinable? %s\n",chunk.joinable()?"true":"false");

        // cout << "Testing start.." << endl;
        // printf("joinable? %s\n",chunk.joinable()?"true":"false");
        // std::this_thread::sleep_for (std::chrono::seconds(3));
        // cout << "Testing End.." << endl;
        // rc = pthread_create(&threads[i], NULL, is_web, &temp);
        // pthread_join(threads[i], NULL);
        // chunk.join();
       
    }
    for (size_t i = 0; i < m_vec.size(); i++)
    {
        std::thread chunk(is_web,&m_vec[i]);
        chunk.detach();
    }
    



    while(g_thread != domains.size()){
        g_thread.load();
    }
    
    // pthread_exit(NULL);
    return EXIT_SUCCESS;
}