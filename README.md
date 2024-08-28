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

For the sake of the project simplicity, NSCOUNT and ARCOUNT are ignored.

### Question section structure

The question section contains a list of questions (usually just 1) that the sender wants to as the receiver. This section is included in both query and reply packets.

|         Field         |  Size   |                                   Description                                   |
| :-------------------: | :-----: | :-----------------------------------------------------------------------------: |
| Question Name (QNAME) | 16 bits | a domain name represented as a sequence of labels (more details provided below) |
| Question Type (QTYPE) | 16 bits |                             the type of the queries                             |
|    Question Class     | 16 bits |                            the class of the queries                             |

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

For the sake of the project simplicity, it only covers type A and type CNAME.
