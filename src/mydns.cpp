#include "mydns.h"

using namespace std;

DNS *DNS::instance = nullptr;

DNS::DNS() {};

void DNS::handle_client(DNSMessage &request, sockaddr_in &clientAddress)
{
    char buffer[BUF_SIZE];
    size_t totalSize;
    int bytesRead;
    vector<string> question_names;
    socklen_t clientAddressLen = sizeof(clientAddress);
    DNSMessage message; // used to send back to clients

    // Make room for new answers;
    request.answers.clear();

    // Collect all the questions in the question section of the request
    for (auto question : request.questions)
    {
        string temp_qname = "";
        for (size_t i = 0; i < question.qname.size(); i++)
            temp_qname += question.qname[i];
        question_names.push_back(temp_qname);
    }

    // Forward each DNS question to servers
    for (auto question_name : question_names)
    {
        string qname = "";
        DNSMessage response;
        DNSMessage forward_message;
        DNSAnswer new_answer;

        construct_header(forward_message, request);
        construct_question(forward_message, question_name);

        // serialize the message and forward to the DNS server
        serialize_message(forward_message, buffer, totalSize, true, false);
        if (sendto(identity.fd, buffer, totalSize, 0, reinterpret_cast<struct sockaddr *>(&identity.forward_addr), sizeof(identity.forward_addr)) == -1)
            perror("Failed to send response");

        // receive the message from the DNS server
        memset(buffer, 0, sizeof(buffer));
        if ((bytesRead = recvfrom(identity.fd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&identity.forward_addr), &identity.addr_len)) == -1)
            ;
        perror("Error receiving data");

        // deserialize the message received from the DNS server
        deserialize_message(response, buffer, bytesRead, true, true);
        construct_answer(request, response);
    }

    construct_message(message, request, true, true);
    // print_DNS_message(request, true, true);
    // print_DNS_message(message, true, true);

    // return;
#ifdef DEBUG
    print_DNS_message(request, true, true);
#endif

    memset(buffer, 0, sizeof(buffer));
    totalSize = 0;
    serialize_message(message, buffer, totalSize, true, true);

    if (sendto(identity.fd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
        perror("Failed to send response");
};

vector<string> DNS::split(string raw_string, string delimeter)
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
};
vector<uint8_t> DNS::encode_string(string raw_string)
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
};

void DNS::serialize_message(const DNSMessage &message, char *buffer, size_t &totalSize, bool includeQuestion, bool includeAnswer)
{
    size_t offset = 0;

    serialize_header(message, buffer, offset);
    if (includeQuestion)
        serialize_question(message, buffer, offset);
    if (includeAnswer)
        serialize_answer(message, buffer, offset);
    // Set the total size
    totalSize = offset;
};

void DNS::serialize_header(const DNSMessage &message, char *&buffer, size_t &offset)
{
    memcpy(buffer + offset, &message.header, sizeof(message.header));
    offset += sizeof(message.header);
};

void DNS::serialize_question(const DNSMessage &message, char *&buffer, size_t &offset)
{
    for (auto question : message.questions)
    {
        memcpy(buffer + offset, question.qname.data(), question.qname.size());
        offset += question.qname.size();

        memcpy(buffer + offset, &question.qtype, sizeof(question.qtype));
        offset += sizeof(question.qtype);

        memcpy(buffer + offset, &question.qclass, sizeof(question.qclass));
        offset += sizeof(question.qclass);
    }
};
void DNS::serialize_answer(const DNSMessage &message, char *&buffer, size_t &offset)
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
};

void DNS::deserialize_message(DNSMessage &message, char *buffer, size_t length, bool includeQuestion, bool includeAnswer)
{
    char *current = buffer;
    deserialize_header(message, buffer, current, length);
    if (includeQuestion)
        deserialize_question(message, buffer, current, length);
    if (includeAnswer)
        deserialize_answer(message, buffer, current, length);
};

void DNS::deserialize_header(DNSMessage &message, char *&buffer, char *&current, size_t length)
{
    // temporary variables used to copy
    uint16_t tmp16;
    uint32_t tmp32;

    // Extract the header section
    // Extract the id
    memcpy(&tmp16, current, sizeof(uint16_t));
    message.header.id = ntohs(tmp16);
    current += sizeof(uint16_t);

    // Extract the QR, OPCODE, AA, TC, RD, RA, A and RCODE
    memcpy(&tmp16, current, sizeof(uint16_t));
    message.header.flags = ntohs(tmp16);
    current += sizeof(uint16_t);

    // Extract QDCOUNT
    memcpy(&tmp16, current, sizeof(uint16_t));
    message.header.qdcount = ntohs(tmp16);
    current += sizeof(uint16_t);

    // Extract ANCOUNT
    memcpy(&tmp16, current, sizeof(uint16_t));
    message.header.ancount = ntohs(tmp16);
    current += sizeof(uint16_t);

    // Extract ARCOUNT
    memcpy(&tmp16, current, sizeof(uint16_t));
    message.header.arcount = ntohs(tmp16);
    current += sizeof(uint16_t);

    while (*current == 0)
        current++; // perhaps there is a null terminator at the end of the header
};

void DNS::deserialize_question(DNSMessage &message, char *&buffer, char *&current, size_t length)
{
    // temporary variables used to copy
    uint16_t tmp16;
    uint32_t tmp32;

    // Extract QNAME
    for (int i = 0; i < message.header.qdcount; ++i)
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

        // Extract TYPE
        memcpy(&tmp16, current, sizeof(uint16_t));
        question.qtype = ntohs(tmp16);
        current += sizeof(uint16_t);

        // Extract QCLASS
        memcpy(&tmp16, current, sizeof(uint16_t));
        question.qclass = ntohs(tmp16);
        current += sizeof(uint16_t);

        message.questions.push_back(question);
    }
};
void DNS::deserialize_answer(DNSMessage &message, char *&buffer, char *&current, size_t length)
{
    // temporary variables used to copy
    uint16_t tmp16;
    uint32_t tmp32;
    const char *end = buffer + length;

    if (current < end)
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

        // Extract TYPE
        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.type = ntohs(tmp16);
        current += sizeof(uint16_t);

        // Extract CLASS
        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.class_ = ntohs(tmp16);
        current += sizeof(uint16_t);

        // Extract TTL (Time-to-live)
        memcpy(&tmp32, current, sizeof(uint32_t));
        new_answer.ttl = ntohl(tmp32);
        current += sizeof(uint32_t);

        // Extract RDLENGTH
        memcpy(&tmp16, current, sizeof(uint16_t));
        new_answer.rdlength = ntohs(tmp16);
        current += sizeof(uint16_t);

        // Extract RDATA
        memcpy(&tmp32, current, sizeof(uint32_t));
        new_answer.rdata = ntohl(tmp32);
        current += sizeof(uint32_t);

        message.answers.push_back(new_answer);
    }
};

void DNS::construct_message(DNSMessage &message, DNSMessage &request, bool includeQuestion, bool includeAnswer)
{
    // Initialize message in struct format
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
        new_question.qname = encode_string(question_name);
        new_question.qtype = htons(1);
        new_question.qclass = htons(1);
        message.questions.push_back(new_question);
    }

    // copy the answer section
    for (auto answer : request.answers)
    {
        DNSAnswer new_answer;
        new_answer.name = answer.name;
        new_answer.type = htons(answer.type);
        new_answer.class_ = htons(answer.class_);
        new_answer.ttl = htonl(answer.ttl);
        new_answer.rdlength = htons(4);
        new_answer.rdata = htonl(answer.rdata);

        message.answers.push_back(new_answer);
    }

    // copy the header section
    message.header.id = htons(request.header.id);

    uint16_t opcode = (request.header.flags & 0b0111100000000000) >> 11;
    request.header.flags &= 0b0111100100000000;
    request.header.flags |= 0b1000000000000000; // default flags: QR(1), AA(0), TC(0), RA(0), Z(0),
    if (opcode != 0)
    {
        request.header.flags |= 0b0000000000000100;
    }

    message.header.flags = htons(request.header.flags);
    message.header.qdcount = htons(question_names.size());
    message.header.ancount = htons(question_names.size());
    message.header.nscount = htons(0);
    message.header.arcount = htons(0);
};
void DNS::construct_header(DNSMessage &message, DNSMessage &request)
{
    message.header.id = htons(request.header.id);
    message.header.flags = 0x0100;
    message.header.qdcount = htons(1);
    message.header.ancount = htons(0);
    message.header.nscount = htons(0);
    message.header.arcount = htons(0);
};

void DNS::construct_question(DNSMessage &message, string question_name)
{
    DNSQuestion new_question;
    new_question.qname = encode_string(question_name);
    new_question.qtype = htons(1);
    new_question.qclass = htons(1);
    message.questions.push_back(new_question);
};

void DNS::construct_answer(DNSMessage &message, DNSMessage &response)
{
    DNSAnswer new_answer;

    string qname = "";
    for (size_t i = 0; i < response.answers[0].name.size(); i++)
        qname += response.answers[0].name[i];

    new_answer.name = encode_string(qname);
    new_answer.type = response.answers[0].type;
    new_answer.class_ = response.answers[0].class_;
    new_answer.ttl = response.answers[0].ttl;
    new_answer.rdlength = response.answers[0].rdlength;
    new_answer.rdata = response.answers[0].rdata;

    message.answers.push_back(new_answer);
};

void DNS::print_DNS_message(DNSMessage &message, bool includeQuestion, bool includeAnswer)
{
    cout << "HEADER SECTION:" << endl;
    cout << "header.id: " << message.header.id << endl;
    cout << "header.flags: ";
    print_binary_form(message.header.flags);
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
};

void DNS::print_hex_form(char *buffer, size_t length)
{
    for (size_t i = 0; i < length; i++)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (static_cast<unsigned>(buffer[i]) & 0xff) << " ";
    std::cout << std::dec << std::endl; // Switch back to decimal for normal output
};

void DNS::print_binary_form(uint16_t flag)
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
};

DNS *DNS::getInstance()
{
    if (instance == nullptr)
        instance = new DNS();
    return instance;
};
int DNS::run(int argc, char **argv)
{
    // Flush after every cout / cerr
    cout << unitbuf;
    cerr << unitbuf;
    cout << argc << endl;
    for (size_t i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], "--resolver", 11) == 0)
        {
            identity.isResolver = true;
            vector<string> res = split(argv[i + 1], ":");
            identity.forward_address = string(res[0]);
            identity.forward_port = atoi(res[1].c_str());
            cout << "forward_address: " << identity.forward_address << endl;
            cout << "forward_port: " << identity.forward_port << endl;

            // Initialize resolver address
            identity.forward_addr.sin_family = AF_INET;
            identity.forward_addr.sin_port = htons(identity.forward_port);
            inet_pton(AF_INET, identity.forward_address.c_str(), &identity.forward_addr.sin_addr);
            identity.addr_len = sizeof(identity.forward_addr);
        }
    }

    // Disable output buffering
    setbuf(stdout, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    cout << "Logs from your program will appear here!" << endl;

    //   Uncomment this block to pass the first stage
    struct sockaddr_in clientAddress;

    identity.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (identity.fd == -1)
    {
        cerr << "Socket creation failed: " << strerror(errno) << "..." << endl;
        return 1;
    }

    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(identity.fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        cerr << "SO_REUSEPORT failed: " << strerror(errno) << endl;
        return 1;
    }

    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(identity.fd, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0)
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
        bytesRead = recvfrom(identity.fd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1)
        {
            perror("Error receiving data");
            break;
        }
        buffer[bytesRead] = '\0';

        DNSMessage request;

        deserialize_message(request, buffer, bytesRead, true, false);
        
#ifdef DEBUG
        print_DNS_message(request, true, false);
#endif

        handle_client(request, clientAddress);
    }

    close(identity.fd);

    return 0;
};