#pragma once
#include "file_utility.hpp"
#include "time_utility.hpp"
#include "net_utility.hpp"
enum PROTOCOL_TYPE {
    UDP = 1,
    TCP = 2
};
struct syslog_config {
    int id = 0;
    int facility_id = 1;
    /*
        Facility Values
        Integer Facility
        0 Kernel messages
        1 User-level messages
        2 Mail system
        3 System daemons
        4 Security/authorization messages
        5 Messages generated internally by Syslogd
        6 Line printer subsystem
        7 Network news subsystem
        8 UUCP subsystem
        9 Clock daemon
        10 Security/authorization messages
        11 FTP daemon
        12 NTP subsystem
        13 Log audit
        14 Log alert
        15 Clock daemon
        16 Local use 0 (local0)
        17 Local use 1 (local1)
        18 Local use 2 (local2)
        19 Local use 3 (local3)
        20 Local use 4 (local4)
        21 Local use 5 (local5)
        22 Local use 6 (local6)
        23 Local use 7 (local7)
    */
    int priority_id = 0;
    const char *server_ip = "127.0.0.1";
    int port = 514;
    int protocol = UDP;
    const char *sender_id = "admin";
    const char *lang = nullptr;
};
class syslog_client {
public:
    syslog_client() = default;
    virtual ~syslog_client() {
        close_fd();
    }
public:
    bool init() {
        if (UDP == config_.protocol) {
            sock_fd_ = G_NET_UTILITY.connect_udp_server(config_.server_ip, config_.port);
        }
        if (sock_fd_ < 0) {
            std::cerr << "sock fd init failed." << std::endl;
            return false;
        }
        header_ = config_.facility_id * 8 + config_.priority_id;
        return true;
    }
    void set_config(const syslog_config &config) {
        config_ = config;
    }
    bool send_syslog_file_by_udp(const char *file_path) {
        send_failed_line_num_ = 0;
        std::vector<std::string>file_content;
        if (false == G_FILE_UTILITY.read_file_content_to_vector(file_path, file_content)) {
            std::cerr << "read file:" << file_path << " failed." << std::endl;
            return false;
        }
        auto cur_time = G_TIME_UTILITY.get_cur_time("%b %d %T");
        for (auto &line : file_content) {
            make_syslog_line(cur_time, line);
            if (write(sock_fd_, line.c_str(), line.size()) < 0) {
                ++send_failed_line_num_;
            }
        }
        return true;
    }
    void close_fd() {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
        }
    }
    inline int get_send_failed_line_num() const {
        return send_failed_line_num_;
    }
private:
    inline void make_syslog_line(const std::string &cur_time, std::string &line) {
        char buf[64] = "";
        snprintf(buf, sizeof(buf), "<%d>%s %s", header_, cur_time.c_str(), config_.sender_id);
        line.insert(0, buf);
    }
private:
    int sock_fd_ = -1;
    int header_ = 0;
    int send_failed_line_num_ = 0;
    syslog_config config_;

};