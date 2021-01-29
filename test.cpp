#include "syslog_client.hpp"
int main() {
    syslog_client client;
    if (client.init()) {
        if (client.send_syslog_file_by_udp("./test.cpp")) {
            std::cout << "send failed num:" << client.get_send_failed_line_num() << std::endl;
        }
    }

    return 0;
}