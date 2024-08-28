# Basic Domain Name System Server

This project is to build a DNS server that's capable of responding to basic DNS queries. This project covers DNS protocol, DNS packet format, DNS record types, UDP servers and more.

## How to run

[under construction]

## A DNS protocol consists of 5 sections: header, question, answer, authority, and an additional space

For more details on DNS packet format, take a look at [Wikipedia](https://en.wikipedia.org/wiki/Domain_Name_System#DNS_message_format) or [RFC 1035](https://tools.ietf.org/html/rfc1035#section-4.1) or [this link](https://github.com/EmilHernvall/dnsguide/blob/b52da3b32b27c81e5c6729ac14fe01fef8b1b593/chapter1.md)

### Header section structure

|               Field               |  Size   |                                                                    Description                                                                     |
| :-------------------------------: | :-----: | :------------------------------------------------------------------------------------------------------------------------------------------------: |
|      Packet Identifier (ID)       | 16 bits |                                A random ID assigned to query packets. Response packets must reply with the same ID.                                |
|   Query/Response Indicator (QR)   |  1 bit  |                                                          1 for a reply, O for a question                                                           |
|      Operation Code (OPCODE)      | 4 bits  | Specifies the kind of query in a message. 0 (a standard query), 1(an inverse query), 2 (a server status request), 3 - 15 (reversed for future use) |
|     Authoritative Answer (AA)     |  1 bit  |                                 1 if the corresponding server "owns" the domain queried, i.e., it's authoritative                                  |
|          Truncation (TC)          |  1 bit  |                                      1 if the message is larger than 512 bytes. Always in 0 in UDP responses                                       |
|      Recursion Desired (RD)       |  1 bit  |                               Sender sets this to 1 if the server should recursively resolve this query, 0 otherwise                               |
|     Recursion Available (RA)      |  1 bit  |                                           Server sets this to 1 to indicate that recursion is available                                            |
|           Reserved (Z)            | 3 bits  |                                        Used by DNSSEC queries. At inception, it was reserved for future use                                        |
|       Response Code (RCODE)       | 4 bits  |                                                Response code indicating the status of the response                                                 |
|     Question Count (QDCOUNT)      | 16 bits |                                                    Number of questions in the Question section                                                     |
|   Answer Record Count (ANCOUNT)   | 16 bits |                                                      Number of records in the Answer section                                                       |
| Authority Record Count (NSCOUNT)  | 16 bits |                                                     Number of records in the Authority section                                                     |
| Additional Record Count (ARCOUNT) | 16 bits |                                                    Number of records in the Additional section                                                     |

For the sake of the project simplicity, NSCOUNT and ARCOUNT are ignored. QR, OPCODE, AA, TC, RD, RA, Z and RCODE are treated as a flag of 16 bits.

### Question section structure

The question section contains a list of questions (usually just 1) that the sender wants to as the receiver. This section is included in both query and reply packets.

|          Field          |  Size   |                                   Description                                   |
| :---------------------: | :-----: | :-----------------------------------------------------------------------------: |
|  Question Name (QNAME)  | 16 bits | a domain name represented as a sequence of labels (more details provided below) |
|  Question Type (TYPE)   | 16 bits |                             the type of the queries                             |
| Question Class (QCLASS) | 16 bits |                            the class of the queries                             |

where:

- labels are like: www.example.com is represented as 3 label including "www", "example" and "com".
- QTYPE:

  | TYPE  | value |                 meaning                  |
  | :---: | :---: | :--------------------------------------: |
  |   A   |   1   |              a host address              |
  |  NS   |   2   |       an authoritative name server       |
  |  MD   |   3   |           a small destination            |
  |  MF   |   4   |            a small forwarder             |
  | CNAME |   5   |     the canonical name for an alias      |
  |  SOA  |   6   |  marks the start of a zone of authority  |
  |  MB   |   7   |   a mailbox domain name (EXPERIMENTAL)   |
  |  MG   |   8   |    a mail group member (EXPERIMENTAL)    |
  |  MR   |   9   | a mail rename domain name (EXPERIMENTAL) |
  | NULL  |  10   |         a null RR (EXPERIMENTAL)         |
  |  WKS  |  11   |     a well known service description     |
  |  PTR  |  12   |          a domain name pointer           |
  | HINFO |  13   |             host information             |
  | MINFO |  14   |     mailbox or mail list information     |
  |  MX   |  15   |              mail exchange               |
  |  TXT  |  16   |               text strings               |

- QCLASS

  | TYPE | value |     meaning      |
  | :--: | :---: | :--------------: |
  |  IN  |   1   |   the Internet   |
  |  CS  |   2   | the CSNET class  |
  |  CH  |   3   | the CHAOS class  |
  |  HS  |   4   | Hesiod [Dyer 87] |

For the sake of the project simplicity, it only covers type A and type CNAME, and class IN.

#### Domain name encoding

Domain names in DNS packets are encoded as a sequence of labels, used in question and answer section.
Labels are encoded as {length}{content}, where length is one byte specifying the length of each label and content is the actual content of the labels

For example:

- <code>google.com</code> is encoded as <code>\x06google\x03com</code> and <code>06 67 6f 6f 67 6c 65 03 63 6f 6d 00</code>
  - <code>\x06google</code> is the first label
    - <code>\x06</code> is a single byte, which is the length of the label
    - <code>google</code> is the content of the label
  - <code>\x03com</code> is the second label
    - <code>\x03</code> is a single byte, which is the length of the label
    - <code>com</code> is the content of the label
  - <code>\x00</code> is the null terminator of the domain name

```cpp
for (int i = 0; i < packet.header.qdcount; ++i)
{
    DNSQuestion question;
    while (*current != 0)
    {
        uint8_t len = *current++;
        question.qname.push_back('.');
        while (len-- > 0)
        {
            question.qname.push_back(*current++);
        }
    }
    // Extract the rest down here
}
```

### Answer section structure

|       Field        |        Size        |                          Description                          |
| :----------------: | :----------------: | :-----------------------------------------------------------: |
|        Name        | sequence of labels |        The domain name encoded as a sequence of labels        |
|        Type        |      16 bits       |              1 for A record, 5 for CNAME record               |
|       Class        |      16 bits       |                        1 for IN class                         |
| TTL (Time-to-live) |      32 bits       | The duration in seconds a record can be cached before expired |
|  Length(Rdlength)  |      16 bits       |                   Length of the RDATA field                   |
|    Data (Rdata)    |      32 bits       |               Data specific to the record type                |

For the sake of the project simplicity, it only deals with the "A" record type, which maps a domain name to an IPv4 address. The RDATA field for an "A" record type is represented as the IPv4 address

#### Parse compressed packet

In DNS (Domain Name System), a compressed packet refers to a technique used within DNS messages to reduce the size of the message by eliminating the repetition of the domain names. This compression scheme is particularly useful for minimizing the bandwidth used by DNS queries and responses, and it is defined in RFC 1035.

This project will deal with this type of packet as well, where a packet may contain many questions at the same time, so the answer section has to be included with many answers as well.

For example, if a dns server receives:

|                  Header                   |
| :---------------------------------------: |
| Question 1 (un-compressed label sequence) |
|  Question 2 (compressed label sequence)   |

then, it has to send back:

|                  Header                   |
| :---------------------------------------: |
| Question 1 (un-compressed label sequence) |
| Question 2 (un-compressed label sequence) |
|  Answer 1 (un-compressed label sequence)  |
|  Answer 2 (un-compressed label sequence)  |

However, the dns only knows there is a compressed content if it sees a pointer to a prior occurrence of the same name. For example, if the pointer is 0c 10, 10 is decoded as 16 in decimal, which means the pointer is pointing to the 16th position counted from the first byte of the packet.

The pointer takes the form of a two octet sequence:
| 1 1| OFFSET |

The first thing we need to determine if we have a compressed content by finding 0x0c hex and then mask out the first two bits of the current byte with 0x3F (0011 1111). After that, we add 8 more bits making room for the OFFSET byte by left-shifting by 8 bits and using OR gate to add the OFFSET bytes.

```cpp
while (*current != 0)
{
    // Check for compression
    if ((*current & 0xC0) == 0xC0)
    {
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
```
