#pragma once
#include "file_utility.hpp"
#include "time_utility.hpp"
#include "net_utility.hpp"
#include "base_thread.hpp"
#include <dirent.h>
#include <thread>
#include <chrono>
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
struct cache_lines {
    cache_lines() {
        lines.resize(capacity);
    }
    inline void set_capacity(int cap) {
        capacity = cap;
    }
    inline bool block() {
        return (read_pos == write_pos);
    }
    inline bool empty() {
        return is_empty;
    }
    inline void append(const std::string &line) {
        lines[write_pos] = line;
        write_pos = (write_pos + 1) % capacity;
        if (is_empty) {
            is_empty = false;
        }
    }
    inline void fetch(std::string &line) {
        read_pos = (read_pos + 1) % capacity;
        line = lines[read_pos];
        if (read_pos == write_pos) {
            is_empty = true;
        }
    }
    int capacity = 1024;
    std::vector<std::string>lines;
    int read_pos = -1;
    int write_pos = 0;
    bool is_empty = true;
};
class read_file_thread : public base_thread {
    virtual void process() override {
        if (!dir_path_) {
            return;
        }
        struct dirent *ptr = nullptr;
        std::string file_path;
        std::string line;
        while (true) {
            DIR *dir = opendir(dir_path_);
            if (!dir) {
                std::cerr << dir_path_ << " open dir failed." << std::endl;
                return;
            }
            while ((ptr = readdir(dir))) {
                if (!strcmp(ptr->d_name, ".") || !strcmp(ptr->d_name, "..")) {
                    continue;
                }
                file_path = dir_path_;
                file_path += "/";
                file_path += ptr->d_name;
                std::ifstream ifs(file_path.c_str(), std::ios::in);
                if (!ifs || !ifs.is_open()) {
                    continue;
                }
                line.clear();
                while (getline(ifs, line)) {
                    if (line.empty()) {
                        continue;
                    }
                    while (cache_lines_.block()) {
                        std::cerr << "read file thread block." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    cache_lines_.append(line);
                }
                ifs.close();
            }
            closedir(dir);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
public:
    read_file_thread(cache_lines &lines) : cache_lines_(lines) {
    }
public:
    inline void set_path(const char *path) {
        dir_path_= path;
    }
private:
    const char *dir_path_ = nullptr;
    cache_lines &cache_lines_;
};
class send_syslog_thread : public base_thread {
    virtual void process() override {
        std::string line;
        header_ = config_.facility_id * 8 + config_.priority_id;
        while (true) {
            if (cache_lines_.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            cache_lines_.fetch(line);
            auto cur_time = G_TIME_UTILITY.get_cur_time("%b %d %T");
            make_syslog_line(cur_time, line);
            if (write(sock_fd_, line.c_str(), line.size()) < 0) {
                ++send_failed_line_num_;
            }
        }
    }
public:
    send_syslog_thread(cache_lines &lines, syslog_config &config, int &fd) : cache_lines_(lines), config_(config), sock_fd_(fd) {
    }
private:
    inline void make_syslog_line(const std::string &cur_time, std::string &line) {
        char buf[64] = "";
        snprintf(buf, sizeof(buf), "<%d>%s %s", header_, cur_time.c_str(), config_.sender_id);
        line.insert(0, buf);
    }
    inline int get_send_failed_line_num() const {
        return send_failed_line_num_;
    }
private:
    int &sock_fd_;
    cache_lines &cache_lines_;
    syslog_config &config_;
    int header_ = 0;
    int send_failed_line_num_ = 0;
};
class syslog_client {
public:
    syslog_client() : read_file_thread_(cache_lines_), send_syslog_thread_(cache_lines_, config_, sock_fd_) {
    }
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
        return true;
    }
    void set_config(const syslog_config &config) {
        config_ = config;
    }
    void set_file_path(const char *path) {
        read_file_thread_.set_path(path);
    }
    inline void set_cache_lines_capacity(int cap) {
        cache_lines_.set_capacity(cap);
    }
    void start_threads() {
        read_file_thread_.run();
        send_syslog_thread_.run();
        read_file_thread_.join();
        send_syslog_thread_.join();
    }
    void close_fd() {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
        }
    }
private:
    int sock_fd_ = -1;
    syslog_config config_;
private:
    cache_lines cache_lines_;
private:
    read_file_thread read_file_thread_;
    send_syslog_thread send_syslog_thread_;
};