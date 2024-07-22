
#include "./config/config.h"
#include "./server/server.h"

int main(int argc, char * argv[]) {
    // ------ 数据库信息配置 ------
    // 登录后端数据库的用户名和密码
    string username = "root";
    string password = "741067";
    string databasename = "yourdb";

    // ------- 命令行配置 -------
    Config config;
    config.parse_arg_(argc, argv);

    // ------ 服务器信息 -------
    Server server;
    // 服务器初始化
    server.server_init(config);


    return 0;
}