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
|       Response Code (RCODE)       | 4 bits  |                                                 Response code indicating the status of the reponse                                                 |
|     Question Count (QDCOUNT)      | 16 bits |                                                    Number of questions in the Question section                                                     |
|   Answer Record Count (ANCOUNT)   | 16 bits |                                                      Number of records in the Answer section                                                       |
| Authority Record Count (NSCOUNT)  | 16 bits |                                                     Number of records in the Authority section                                                     |
| Additional Record Count (ARCOUNT) | 16 bits |                                                    Number of records in the Additional section                                                     |
