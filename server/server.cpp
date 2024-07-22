#include "server.h"

Server::Server() {

}

Server::~Server() {

}

bool Server::server_init(Config config) {
    m_port = config.port;
    m_sql_thread_num = config.sql_thread_num;
    m_conn_thread_num = config.conn_thread_num;
    m_log_open = config.log_open;
    m_log_write_way = config.log_write_way;
    m_socket_linger_opt = config.socket_linger_opt;
    m_actor_mode = config.actor_mode;
    m_lfd_trig_mode = config.lfd_trig_mode;
    m_cfd_trig_mode = config.cfd_trig_mode;
}


void Server::server_init(string username, string password, string databasename) {
    // 这里只赋值数据库的几个内容
    m_username = username;
    m_password = password;
    m_database_name = databasename;
}