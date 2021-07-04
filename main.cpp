#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <pthread.h>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using namespace std;
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

#define NUM_THREADS 60

struct Domain
{
    string host;
    string port;
};

vector<Domain> split_vec(vector<Domain> domains, size_t start, size_t size, size_t vec_size)
{
    vector <Domain> temp;
    for (size_t i = start; i <= size; i++)
    {
        if (i < vec_size)
        {
            // cout << "start: " << start << " " << domains[i].host << endl;
            temp.push_back(domains[i]);
        }
    }
    return temp;
}

void *is_web(void *d1)
{
    vector<Domain> *domains_vector = (vector<Domain> *)d1;
    // printf("vec size: %d\n", domains_vector->size());
    for (auto d : *domains_vector)
    {
        string sub_host = d.host;
        string port = d.port;
        // cout << sub_host << endl;
        auto const target = "/";
        int version = 11;
        try
        {
            // The io_context is required for all I/O
            net::io_context ioc;

            // These objects perform our I/O
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);

            // Look up the domain name
            auto const results = resolver.resolve(sub_host, port);
            // cout << results << endl;

            // // Make the connection on the IP address we get from a lookup
            stream.connect(results);

            // Set the logical operation timer to 100 milliseconds.
            stream.expires_after(std::chrono::milliseconds(100));

            // // Set up an HTTP GET request message
            // http::request<http::string_body> req{http::verb::head, target, version};
            // req.set(http::field::host, sub_host);
            // req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            // // // Send the HTTP request to the remote host
            // http::write(stream, req);

            // // // This buffer is used for reading and must be persisted
            // beast::flat_buffer buffer;

            // // // Declare a container to hold the response
            // http::response<http::dynamic_body> res;

            // // // Receive the HTTP response
            // http::read(stream, buffer, res);

            // // // Write the message to standard out
            // std::cout << res.has_content_length() << std::endl;
            // // cout << res.has_content_length() << endl;
            // http::status stats_code = res.result();
            // cout << stats_code << endl;

            // // Gracefully close the socket
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            // // not_connected happens sometimes
            // // so don't bother reporting it.
            // //
            if (ec && ec != beast::errc::not_connected)
                throw beast::system_error{ec};
            else
            {
                printf("[+] https://%s\n", sub_host.c_str());
                // return true;
            }

            // If we get here then the connection is closed gracefully
            stream.close();
        }
        catch (std::exception const &e)
        {
            printf("[-] \t%s - %s\n", sub_host.c_str(), e.what());
            // return EXIT_FAILURE;
            // return false;
        }
        // pthread_exit(NULL);
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
    
    // vector <vector<Domain>> m_vec;
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        // printf("%d - %d - %d\n",i,current_index,chunk_end-1);
        current_index += chunk_size;
        chunk_end += chunk_size;
        vector<Domain> temp;
        temp = split_vec(domains, current_index, chunk_end - 1, domains.size());
        rc = pthread_create(&threads[i], NULL, is_web, &temp);
        pthread_join(threads[i], NULL);
        // m_vec.push_back(temp);
    }
    // for (int i=0; i<NUM_THREADS; i++){
    // 	pthread_join(threads[i], NULL);
    // }

    //  for (size_t i = 0; i < NUM_THREADS; i++)
    // {
    //     rc = pthread_create(&threads[i], NULL, is_web, (void *)&temp[i]);
    //     if (rc)
    //     {
    //         cout << "Error:unable to create thread," << rc << endl;
    //         exit(-1);
    //     }
    // }

    // (void *)&domains[i
    // for (int i = 0; i < NUM_THREADS; i++)
    // {

    //         rc = pthread_create(&threads[i], NULL, is_web, (void *)&domains[i]);
    // if (is_web(sub_host, "80"))
    // {
    //     printf("[+] http://%s\n", sub_host.c_str());
    // }
    // else if (is_web(sub_host, "443"))
    // {
    //     printf("[+] https://%s\n", sub_host.c_str());
    // }
    // else
    // {
    //     // printf("[-]\t%s - Failed!\n", sub_host.c_str());
    // }
    // if (rc)
    // {
    //     cout << "Error:unable to create thread," << rc << endl;
    //     exit(-1);
    // }

    // }
    // for (int i=0; i<NUM_THREADS; i++){
    // 	pthread_join(threads[i], NULL);
    // }

    pthread_exit(NULL);
    return EXIT_SUCCESS;
}