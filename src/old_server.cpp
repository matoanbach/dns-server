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

string resolver_address_string;
int resolver_port;
struct myaddr;
struct resolver_server;
bool use_resolve = false;

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

void printDNSMessage(DNSMessage &message, bool includeQuestion, bool includeAnswer);
void hex_form(char *buffer, size_t length);
vector<string> split(string raw_string, string delimeter);
void serialize_message(const DNSMessage &message, char *buffer, size_t &totalSize, bool includeAnswer);
vector<uint8_t> encode_string(string raw_string);
DNSMessage construct_message(DNSMessage &request);
DNSMessage deserialize_message(char *buffer, size_t length, bool includeAnswer);
void binary_form(uint16_t flag);

void resolve(DNSMessage &request, sockaddr_in &clientAddress, int udpSocket);

DNSMessage construct_question(DNSMessage &request, string question_name);

int main(int argc, char **argv)
{
    // Flush after every cout / cerr
    cout << unitbuf;
    cerr << unitbuf;
    cout << argc << endl;
    for (size_t i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], "--resolver", 11) == 0)
        {
            use_resolve = true;
            vector<string> res = split(argv[i + 1], ":");
            resolver_address_string = string(res[0]);
            resolver_port = atoi(res[1].c_str());
            cout << "resolver_address_string: " << resolver_address_string << endl;
            cout << "resolver_port: " << resolver_port << endl;
        }
    }

    // Disable output buffering
    setbuf(stdout, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    cout << "Logs from your program will appear here!" << endl;

    //   Uncomment this block to pass the first stage
    int udpSocket;
    struct sockaddr_in clientAddress;

    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1)
    {
        cerr << "Socket creation failed: " << strerror(errno) << "..." << endl;
        return 1;
    }

    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        cerr << "SO_REUSEPORT failed: " << strerror(errno) << endl;
        return 1;
    }

    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(udpSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0)
    {
        cerr << "Bind failed: " << strerror(errno) << endl;
        return 1;
    }

    int bytesRead;
    char buffer[2048];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true)
    {
        // Receive data
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1)
        {
            perror("Error receiving data");
            break;
        }
        buffer[bytesRead] = '\0';

        // cout << "request: ";
        // hex_form(buffer, bytesRead);
        // cout << endl;
        // Print the package received
        DNSMessage request = deserialize_message(buffer, bytesRead, false);
        // hex_form(buffer, bytesRead);
        printDNSMessage(request, true, false);

        // make a response
        char response[2048];
        size_t totalSize;
        DNSMessage message = construct_message(request);

        // test
        if (use_resolve)
        {
            // serialize_message(request, response, totalSize, true);
            resolve(request, clientAddress, udpSocket);
        }
        else
        {
            // serialize message into the response
            serialize_message(message, response, totalSize, true);
            // cout << "response: ";
            // hex_form(response, totalSize);
            // cout << endl;
            // Send response
            if (sendto(udpSocket, response, sizeof(response), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
            {
                perror("Failed to send response");
            }
        }
    }

    close(udpSocket);

    return 0;
}

DNSMessage construct_question(DNSMessage &request, string question_name)
{
    DNSMessage message;
    // copy the header section
    message.header.id = htons(request.header.id);
    message.header.flags = 0x0100;
    message.header.qdcount = htons(1);
    message.header.ancount = htons(0);
    message.header.nscount = htons(0);
    message.header.arcount = htons(0);

    DNSQuestion new_question;
    new_question.qname = encode_string(question_name);
    new_question.qtype = htons(1);
    new_question.qclass = htons(1);
    message.questions.push_back(new_question);
    return message;
}

void resolve(DNSMessage &request, sockaddr_in &clientAddress, int udpSocket)
{
    // printDNSMessage(request, true, true);
    // shared variables
    char buffer[2048];
    size_t totalSize;

    // variable used to response back after forwarding
    // DNSMessage responded_message;
    // DNSAnswer responded_answer;
    // empty the answer section, making room for new ones
    request.answers.clear();

    // Initialize resolver address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(resolver_port);
    inet_pton(AF_INET, resolver_address_string.c_str(), &addr.sin_addr);
    socklen_t addr_len = sizeof(addr);

    // Initialize message in struct format
    vector<string> question_names;
    socklen_t clientAddrLen = sizeof(clientAddress);
    // copy the question section

    for (auto question : request.questions)
    {
        string temp_qname = "";
        for (size_t i = 1; i < question.qname.size(); i++)
            temp_qname += question.qname[i];
        question_names.push_back(temp_qname);
    }

    // this section is to forward message if we're a resolver
    for (auto question_name : question_names)
    {
        DNSMessage forward_message = construct_question(request, question_name);
        DNSMessage res;
        DNSAnswer new_answer;
        string qname = "";

        // serialize the message and forward to the DNS server
        serialize_message(forward_message, buffer, totalSize, false);
        if (sendto(udpSocket, buffer, totalSize, 0, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
            perror("Failed to send response");

        // receive the message from the DNS server and then deserialize it
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&addr), &addr_len);
        if (bytesRead == -1)
            perror("Error receiving data");

        // construct the questions after done gathering answers
        res = deserialize_message(buffer, bytesRead, true);

        for (size_t i = 1; i < res.answers[0].name.size(); i++)
            qname += res.answers[0].name[i];

        new_answer.name = encode_string(qname);
        new_answer.type = res.answers[0].type;
        new_answer.class_ = res.answers[0].class_;
        new_answer.ttl = res.answers[0].ttl;
        new_answer.rdlength = res.answers[0].rdlength;
        new_answer.rdata = res.answers[0].rdata;

        request.answers.push_back(new_answer);
    }
    // request.header.ancount = htons(request.header.qdcount);
    // construct the message used to response to the original requester
    DNSMessage message = construct_message(request);

    memset(buffer, 0, sizeof(buffer));
    totalSize = 0;
    serialize_message(message, buffer, totalSize, true);

    printDNSMessage(message, true, true);
    // cout << "response: ";
    // hex_form(buffer, totalSize);
    // cout << endl;

    if (sendto(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
    {
        perror("Failed to send response");
    }
}

vector<string> split(string raw_string, string delimeter)
{
    vector<string> res;
    size_t start = 0, end = raw_string.find(delimeter);
    while (end != string::npos)
    {
        if (raw_string.substr(start, end - start).size() > 0)
            res.push_back(raw_string.substr(start, end - start));
        start = end + 1;
        end = raw_string.find(delimeter, start);
    }
    res.push_back(raw_string.substr(start));
    return res;
}

vector<uint8_t> encode_string(string raw_string)
{
    vector<string> strs = split(raw_string, ".");
    vector<uint8_t> res;
    for (auto e : strs)
    {
        res.push_back(static_cast<uint8_t>(e.size()));
        for (size_t i = 0; i < e.size(); i++)
            res.push_back(static_cast<uint8_t>(e[i]));
    }
    res.push_back(0);
    return res;
}

void serialize_message(const DNSMessage &message, char *buffer, size_t &totalSize, bool includeAnswer = false)
{
    size_t offset = 0;

    // Copy header
    memcpy(buffer + offset, &message.header, sizeof(message.header));
    offset += sizeof(message.header);

    // Copy question section
    for (auto question : message.questions)
    {

        memcpy(buffer + offset, question.qname.data(), question.qname.size());
        offset += question.qname.size();

        memcpy(buffer + offset, &question.qtype, sizeof(question.qtype));
        offset += sizeof(question.qtype);

        memcpy(buffer + offset, &question.qclass, sizeof(question.qclass));
        offset += sizeof(question.qclass);
    }

    // Copy answer section
    if (includeAnswer)
    {
        for (auto answer : message.answers)
        {

            memcpy(buffer + offset, answer.name.data(), answer.name.size());
            offset += answer.name.size();

            memcpy(buffer + offset, &answer.type, sizeof(answer.type));
            offset += sizeof(answer.type);

            memcpy(buffer + offset, &answer.class_, sizeof(answer.class_));
            offset += sizeof(answer.class_);

            memcpy(buffer + offset, &answer.ttl, sizeof(answer.ttl));
            offset += sizeof(answer.ttl);

            memcpy(buffer + offset, &answer.rdlength, sizeof(answer.rdlength));
            offset += sizeof(answer.rdlength);

            memcpy(buffer + offset, &answer.rdata, sizeof(answer.rdata));
            offset += sizeof(answer.rdata);
        }
    }
    // Set the total size
    totalSize = offset;
}

DNSMessage construct_message(DNSMessage &request)
// DNSMessage construct_message()
{
    // Initialize message in struct format
    DNSMessage response;
    vector<string> question_names;
    // copy the question section
    for (auto question : request.questions)
    {
        string temp_qname = "";
        for (size_t i = 1; i < question.qname.size(); i++)
            temp_qname += question.qname[i];
        question_names.push_back(temp_qname);
    }

    for (auto question_name : question_names)
    {
        DNSQuestion new_question;
        DNSAnswer new_answer;

        new_question.qname = encode_string(question_name);
        new_question.qtype = htons(1);
        new_question.qclass = htons(1);
        response.questions.push_back(new_question);

        if (!use_resolve)
        {
            new_answer.name = encode_string(question_name);
            new_answer.type = htons(1);
            new_answer.class_ = htons(1);
            new_answer.ttl = htonl(60);

            string temp_addr = "8.8.8.8";
            struct in_addr addr;
            inet_pton(AF_INET, temp_addr.c_str(), &addr);

            new_answer.rdlength = htons(4);
            new_answer.rdata = addr.s_addr;

            response.answers.push_back(new_answer);
        }
    }

    // copy the answer section
    for (auto answer : request.answers)
    {
        DNSAnswer new_answer;
        if (use_resolve)
        {
            new_answer.name = answer.name;
            new_answer.type = htons(answer.type);
            new_answer.class_ = htons(answer.class_);
            new_answer.ttl = htonl(answer.ttl);
            new_answer.rdlength = htons(4);
            new_answer.rdata = htonl(answer.rdata);

            response.answers.push_back(new_answer);
        }
    }

    // copy the header section
    response.header.id = htons(request.header.id);

    uint16_t opcode = (request.header.flags & 0b0111100000000000) >> 11;
    request.header.flags &= 0b0111100100000000;
    request.header.flags |= 0b1000000000000000; // default flags: QR(1), AA(0), TC(0), RA(0), Z(0),
    if (opcode != 0)
    {
        request.header.flags |= 0b0000000000000100;
    }

    response.header.flags = htons(request.header.flags);
    response.header.qdcount = htons(question_names.size());
    response.header.ancount = htons(question_names.size());
    response.header.nscount = htons(0);
    response.header.arcount = htons(0);

    return response;
}

/**
 * @brief Print the buffer string in hex format.
 * @param buffer Raw buffer string received from clients.
 * @param length Size of raw buffer string received from clients.
 */
void hex_form(char *buffer, size_t length)
{
    for (size_t i = 0; i < length; i++)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (static_cast<unsigned>(buffer[i]) & 0xff) << " ";
    // std::cout << std::hex << std::setw(3) << (static_cast<unsigned>(buffer[i]) & 0xff) << " ";

    std::cout << std::dec << std::endl; // Switch back to decimal for normal output
}

void binary_form(uint16_t flag)
{
    for (int i = 15; i >= 0; i--)
    {
        cout << ((flag >> i) & 1);
        if (i % 4 == 0 && i != 0)
        {
            cout << " ";
        }
    }
    cout << endl;
}

DNSMessage deserialize_message(char *buffer, size_t length, bool includeAnswer)
{
    char *current = buffer;
    const char *end = buffer + length;
    DNSMessage packet;

    uint16_t tmp16;
    uint32_t tmp32;

    // Extract the header
    memcpy(&tmp16, current, sizeof(uint16_t));
    packet.header.id = ntohs(tmp16);
    current += sizeof(uint16_t);

    memcpy(&tmp16, current, sizeof(uint16_t));
    packet.header.flags = ntohs(tmp16);
    current += sizeof(uint16_t);

    memcpy(&tmp16, current, sizeof(uint16_t));
    packet.header.qdcount = ntohs(tmp16);
    current += sizeof(uint16_t);

    memcpy(&tmp16, current, sizeof(uint16_t));
    packet.header.ancount = ntohs(tmp16);
    current += sizeof(uint16_t);

    memcpy(&tmp16, current, sizeof(uint16_t));
    packet.header.arcount = ntohs(tmp16);
    current += sizeof(uint16_t);

    while (*current == 0)
        current++; // perhaps there is a null terminator at the end of the header

    // hex_form(current, end - current);
    for (int i = 0; i < packet.header.qdcount; ++i)
    {
        DNSQuestion question;
        while (*current != 0)
        {
            if ((*current & 0xC0) == 0xC0)
            { // Check for compression
                /*
                (*current & 0x3F) << 8: This masks out the first two bits of the current byte
                                        (which are not part of the offset) and shifts the remaining
                                        6 bits to the left by 8 bits to make room for the next byte.

                static_cast<uint8_t>(*(current + 1)): This adds the next byte to the offset.
                                        The static_cast<uint8_t> ensures you're working with a
                                        byte-sized integer.
                 */
                uint16_t offset = ((*current & 0x3F) << 8) | static_cast<uint8_t>(*(current + 1));
                cout << "offset: " << offset << endl;
                current = buffer + offset; // Jump to the offset in the buffer
            }
            else
            {
                uint8_t len = *current++;
                question.qname.push_back('.');
                while (len-- > 0)
                {
                    question.qname.push_back(*current++);
                }
            }
        }
        current++; // Skip the null terminator

        memcpy(&tmp16, current, sizeof(uint16_t));
        question.qtype = ntohs(tmp16);
        current += sizeof(uint16_t);

        memcpy(&tmp16, current, sizeof(uint16_t));
        question.qclass = ntohs(tmp16);
        current += sizeof(uint16_t);

        packet.questions.push_back(question);
    }
    // extract answer if there is any
    if (includeAnswer && current < end)
    {
        DNSAnswer new_answer;
        while (*current != 0)
        {
            uint8_t len = *current++;
            new_answer.name.push_back('.');
            while (len-- > 0)
            {
                new_answer.name.push_back(*current++);
            }
        }
        current++; // Skip the null terminator

        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.type = ntohs(tmp16);
        current += sizeof(uint16_t);

        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.class_ = ntohs(tmp16);
        current += sizeof(uint16_t);

        memcpy(&tmp32, current, sizeof(uint32_t));
        new_answer.ttl = ntohl(tmp32);
        current += sizeof(uint32_t);

        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.rdlength = ntohs(tmp16);
        current += sizeof(uint16_t);

        memcpy(&tmp32, current, sizeof(uint32_t));
        new_answer.rdata = ntohl(tmp32);
        current += sizeof(uint32_t);

        packet.answers.push_back(new_answer);
    }

    return packet;
}

void printDNSMessage(DNSMessage &message, bool includeQuestion, bool includeAnswer)
{
    cout << "HEADER SECTION:" << endl;
    cout << "header.id: " << message.header.id << endl;
    cout << "header.flags: ";
    binary_form(message.header.flags);
    cout << endl;
    cout << "header.qdcount: " << message.header.qdcount << endl;
    cout << "header.ancount: " << message.header.ancount << endl;
    cout << "header.nscount: " << message.header.nscount << endl;
    cout << "header.arcount: " << message.header.arcount << endl
         << endl;

    if (includeQuestion && message.questions.size() > 0)
    {
        cout << "QUESTION SECTION: " << endl;
        for (auto question : message.questions)
        {
            cout << "question.qname: ";
            for (auto e : question.qname)
                cout << e;
            cout << endl;
            cout << "question.qtype: " << question.qtype << endl;
            cout << "question.qclass: " << question.qclass << endl
                 << endl;
        }
    }
    if (includeAnswer && message.answers.size() > 0)
    {
        cout << "ANSWER SECTION: " << endl;
        for (auto answer : message.answers)
        {
            cout << "answer.name: " << endl;
            for (auto e : answer.name)
                cout << e;
            cout << endl;
            cout << "answer.type: " << answer.type << endl;
            cout << "answer.class_: " << answer.class_ << endl;
            cout << "answer.ttl: " << answer.ttl << endl;
            cout << "answer.rdlength: " << answer.rdlength << endl;
            cout << "answer.rdata: ";
            // Extract individual bytes
            uint8_t first = (answer.rdata >> 24) & 0xFF;
            uint8_t second = (answer.rdata >> 16) & 0xFF;
            uint8_t third = (answer.rdata >> 8) & 0xFF;
            uint8_t fourth = answer.rdata & 0xFF;

            // Print the IP address
            printf("%d.%d.%d.%d\n", first, second, third, fourth);
        }
    }
}
