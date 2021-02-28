#include "syslog_client.hpp"
int main() {
    syslog_client client;
    client.set_file_path("./");
    if (client.init()) {
        client.start_threads();
    }

    return 0;
}