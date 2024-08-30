#include "mydns.h"

int main(int argc, char **argv)
{
    DNS *dns_server = DNS::getInstance();
    int ret = dns_server->run(argc, argv);
    return ret;
}