#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iomanip>
#include <vector>

#include <sstream>

using namespace std;

const int BUF_SIZE = 2048;
const int default_port = 2053;
const char default_addr[] = "127.0.0.1";

struct DNSHeader
{
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct DNSQuestion
{
    vector<uint8_t> qname;
    uint16_t qtype;
    uint16_t qclass;
};

struct DNSAnswer
{
    vector<uint8_t> name; // an owner name, i.e., the name of the node to which this resource record pertains.
    uint16_t type;        // two octets containing one of the RR TYPE codes.
    uint16_t class_;      // two octets containing one of the RR CLASS codes.
    uint32_t ttl;         /*
                        a 32 bit signed integer that specifies the time interval
                        that the resource record may be cached before the source
                        of the information should again be consulted.  Zero
                        values are interpreted to mean that the RR can only be
                        used for the transaction in progress, and should not be
                        cached.  For example, SOA records are always distributed
                        with a zero TTL to prohibit caching.  Zero values can
                        also be used for extremely volatile data.
                        */

    uint16_t rdlength; // an unsigned 16 bit integer that specifies the length in octets of the RDATA field
    uint32_t rdata;    /*
                   a variable length string of octets that describes the
                   resource.  The format of this information varies
                   according to the TYPE and CLASS of the resource record.
                   */
};

struct DNSMessage
{
    DNSHeader header;
    vector<DNSQuestion> questions;
    vector<DNSAnswer> answers;
};

struct Identity
{
    bool isResolver = false;
    struct sockaddr_in forward_addr;
    string forward_address;
    int forward_port;
    socklen_t addr_len;
    int fd;
};

class DNS
{

    struct myaddr;
    Identity identity;

    void handle_client(DNSMessage &request, sockaddr_in &clientAddress);

    vector<string> split(string raw_string, string delimeter);
    vector<uint8_t> encode_string(string raw_string);

    void serialize_message(const DNSMessage &message, char *buffer, size_t &totalSize, bool includeQuestion, bool includeAnswer);
    void serialize_header(const DNSMessage &message, char *&buffer, size_t &offset);
    void serialize_question(const DNSMessage &message, char *&buffer, size_t &offset);
    void serialize_answer(const DNSMessage &message, char *&buffer, size_t &offset);

    void deserialize_message(DNSMessage &message, char *buffer, size_t length, bool includeQuestion, bool includeAnswer);
    void deserialize_header(DNSMessage &message, char *&buffer, char *&current, size_t length);
    void deserialize_question(DNSMessage &message, char *&buffer, char *&current, size_t length);
    void deserialize_answer(DNSMessage &message, char *&buffer, char *&current, size_t length);

    void construct_message(DNSMessage &message, DNSMessage &request, bool includeQuestion, bool includeAnswer);
    void construct_header(DNSMessage &message, DNSMessage &request);
    void construct_question(DNSMessage &message, string question_name);
    void construct_answer(DNSMessage &message, DNSMessage &response);

    void print_DNS_message(DNSMessage &message, bool includeQuestion, bool includeAnswer);
    void print_hex_form(char *buffer, size_t length);
    void print_binary_form(uint16_t flag);

    DNS();

    static DNS *instance;

public:
    static DNS *getInstance();
    int run(int argc, char ** argv);
};