#include "http_conn.h"


// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

Http_conn::Http_conn(){}
Http_conn::~Http_conn(){}

// 类内定义 类外初始化
int Http_conn::m_epfd = -1;
int Http_conn::m_user_cout = 0;

// 网站资源的根目录
const char * doc_root = "/home/lxh/webserver/resources";

// 设置某个文件描述符为非阻塞
int set_nonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
    return old_flag;
}

// 向epoll中添加需要检测的文件描述符
void addfd(int epfd, int fd, bool one_shot){
    epoll_event epev;
    epev.data.fd = fd;
    // 对于对方连接断开，会触发EPOLLRDHUP，不需要返回值来判断了，而是通过返回事件判断
    epev.events = EPOLLIN | EPOLLRDHUP; // 默认水平触发模式 one-shot用于防止
    // epev.events = EPOLLIN | EPOLLRDHUP | EPOLLET; // 默认水平触发模式 one-shot用于防止
    if(one_shot) {
        epev.events |= EPOLLONESHOT;
    }
    // 追加到epoll中
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &epev);
    // 设置通信文件描述符非阻塞（边沿模式就要非阻塞）
    set_nonblocking(fd);
}

// 从epoll中修改监听的文件描述符，主要是要重置epoll的events属性（EPOLLONESHOT事件，确保下一次可读EPOLLIN可触发）
void modifyfd(int epfd, int fd, int ev) {
    epoll_event epev;
    epev.data.fd = fd;
    epev.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &epev);
}

// 从epoll中移除监听的文件描述符
void removefd(int epfd, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd); // 关闭文件描述符
}

void Http_conn::init() {
    m_checked_state = CHECK_STATE_REQUESTLINE; // 初始化状态为解析请求首行
    m_start_line = 0; // 当前正在解析的索引解析为0
    m_checked_index = 0; // 解析到的位置也初始化为0
    m_read_index = 0; // 读缓冲区的索引也初始化为0
    m_content_length = 0; // 请求体长度置为0
    m_read_index = 0;
    m_write_index = 0;

    m_url = 0;
    m_method = GET;
    m_version = 0;
    m_linger = false;
    m_host = 0;

    // 读缓冲区数据清空
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, READ_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_MAX);
}

void Http_conn::init(int sockfd, const sockaddr_in &addr) {
    m_sockfd = sockfd; // 初始化
    m_addr = addr;

    // 设置通信的文件描述符端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    // 将通信文件描述符添加到epoll中
    addfd(m_epfd, m_sockfd, true);
    m_user_cout++; // 总用户数+1

    init();
}

// 关闭连接
void Http_conn::close_conn() {
    if(m_sockfd != -1) {
        // 关闭一个正常的文件描述符的时候，要考虑到多种操作
        removefd(m_epfd, m_sockfd);
        m_sockfd = -1;
        m_user_cout--;
    }
}

// 循环读取客户数据，直到读取完毕或者没有数据可以读取
bool Http_conn::read() {
    // 如果读的下标已经超过了读缓冲区大小，就说明读完了
    if(m_read_index >= READ_BUFFER_SIZE) {
        return false;
    }
    
    // 读取到的字节
    int bytes_read = 0;
    while(true) {
        // 这一句话的意思，是每次读取从下标位置开始读，读取
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // 标识没有数据了
                break;
            }
            return false;
        } else if(bytes_read == 0) {
            // 对方关闭连接
            return false;
        }
        m_read_index += bytes_read;
    }
    // printf("读取到数据： %s\n", m_read_buf);
    return true;
} 

// 写数据，写到写缓冲区中
bool Http_conn::write() {
    int temp = 0;
    int bytes_have_send = 0; // 已经发送的字节
    int bytes_to_send = m_write_index; // 将要发送的字节（m_write_index）下一个要发送的

    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modifyfd(m_epfd, m_sockfd, EPOLLIN); 
        init();
        return true;
    }

    while(1) {
        // 分散写(多块不连续的内存也可以写入，因为我们的write_buf响应头和响应内容m_address不在一个地方)
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if(errno == EAGAIN) {
                modifyfd(m_epfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send) {
            // 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger) {
                init();
                modifyfd(m_epfd, m_sockfd, EPOLLIN);
                return true;
            } else {
                modifyfd(m_epfd, m_sockfd, EPOLLIN);
                return false;
            } 
        }
    }
}

// 要分析目标文件的属性，即通过url找到资源然后写给客户端
Http_conn::HTTP_CODE Http_conn::do_request() {
    // 比如解析请求头后，服务器得到了资源的相对地址，就需要找到对应的资源
    strcpy(m_real_file, doc_root); // 先把doc_root的路径拷贝到real_file中
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, MAX_FILENAME - len - 1); // 继续把m_url拼接进去
    // 判断m_real_file的相关状态信息
    if(stat(m_real_file, &m_file_stat) < 0) {
        return NO_REQUEST;
    }
    // 判断m_real_file是否有读的访问权限
    if(!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    // 判断是否是目录
    if(S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    // 以只读的方式打开文件
    int fd = open(m_real_file, O_RDONLY);
    // 创建内存映射 文件会被直接映射到内存，对该内存进行操作也就是对文件操作了
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

// 释放内存映射
void Http_conn::unmap(){
    if(m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 解析具体的某一行 - 从状态机 - 根据\n获取的
Http_conn::LINE_STATUS Http_conn::parse_line(){
    // 获取一行数据 结束标志\r\n
    char temp; // 临时字符
    // 循环对检查索引递增，直到读的索引
    for(; m_checked_index < m_read_index; ++m_checked_index) {
        temp = m_read_buf[m_checked_index]; // 字符赋值给temp
        if(temp == '\r') { // 如果temp为\r就判断下一个是否是\n
            // 如果说当前的是\r，但下一个没有了，是下一次读取的索引了
            if((m_checked_index + 1) == m_read_index) {
                return LINE_OPEN;
            } else if((m_read_buf[m_checked_index + 1] == '\n')) {
                // 如果是，就将\r\n的\r和\n都变为\0
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if(temp == '\n') {
            // 按道理来说，\r之后的\n是直接被上一步就置为\0的，这只能是两个连续的\n
            // 不过这里还是进行判断了
            if((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r')) {
                m_read_buf[m_checked_index-1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析请求首行 - 解析请求分开写
// 获得请求方法，目标URL，HTTP版本信息
Http_conn::HTTP_CODE Http_conn::parse_request_line(char * text){
    // text是一行的数据 GET / HTTP/1.1
    // 这里利用字符串或者正则表达式使用
    // 查找text中 空格和\t的第一个位置，首先找到了text + 3
    m_url = strpbrk(text, " \t");
    if(!m_url) {
        return BAD_REQUEST;
    }

    *m_url++ = '\0'; // GET\0/ HTTP/1.1
    char * method = text; // 因为后面有一个\0了所以直接获取就是了
    if(strcasecmp(method, "GET") == 0) {
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    // 获取版本
    // m_url = / HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if(!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0'; // /\0HTTP/1.1
    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    // 判断ur前面是否是http://192.168.1.1:10000/index.html这种
    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7; // 如果是，则移动m_url
        m_url = strchr(m_url, '/'); // 找m_url第一次出现"/"的位置
    }

    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    m_checked_state = CHECK_STATE_HEADER; // 检查状态变为检查请求头 
    return NO_REQUEST;
}

// 解析请求头
Http_conn::HTTP_CODE Http_conn::parse_headers(char * text){
    // 如果遇到空行，说明头部解析完毕
    if(text[0] == '\0') {
        // 如果HTTP请求有消息体，还需要再读取一下m_content_length字节的消息体
        if(m_content_length != 0) {
            m_checked_state = CHECK_STATE_CONTENT; // 状态机，如果就剩请求体没转，则转换到STATE_CONTENT状态
            return NO_REQUEST; // 返回还没有解析完毕呢
        }
        // 否则说明m_content_length=0说明没有请求体了
        return GET_REQUEST; // 返回一个完整的请求
    } else if(strncasecmp(text, "Connection:", 11) == 0) {
        // 处理Connection头部字段 Connection: keep-alive
        text += 11; // text往后移动11个位置，到数据开始位置的空格处
        // strspn函数用于查找第一个字符串的前n个都属于第二个字符串的子串
        // 这里就是为了将空格和\t给去除，直接定位到数据位置
        text += strspn(text, " \t");

        if(strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        // 处理Content-Length头部
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text); // string to long int
    } else if(strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        printf("Ops! unknow header %s\n", text);
    }
    return NO_REQUEST;
}

// 解析请求体
Http_conn::HTTP_CODE Http_conn::parse_content(char * text){
    // 这里不对请求体进行真正的解析，而是只判断请求体是否有
    if(m_read_index >= (m_content_length + m_checked_index)) {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 解析HTTP请求 - 主状态机
Http_conn::HTTP_CODE Http_conn::process_read() {
    // 主状态机的目的是对请求进行解析成三个部分，然后各自送到请求中解析返回
    LINE_STATUS line_status = LINE_OK; // 行状态OK
    HTTP_CODE ret = NO_REQUEST;
    char * text = 0;
    // 怎么解析成三部分，一行一行的读取先
    // 情况1：主状态机的状态是解析内容，以及当前解析行状态为OK
    // 情况2：获取了一行数据解析，并且解析后返回的数据为Line_OK
    while(((m_checked_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
                                || ((line_status = parse_line()) == LINE_OK)) {
        // 解析到了一行完整的数据，或者解析到了请求体，也是完整的数据
        text = get_line();
        // 修改起始下标
        m_start_line = m_checked_index;
        // printf("got 1 http line : %s\n", text);

        switch(m_checked_state) {
            // 解析首行
            case CHECK_STATE_REQUESTLINE: {
                //printf("正在分析请求首行...\n");
                // 语法错误就直接不解析了
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                // 后面还有很多其他可能，这里不写了
                break;
            }
            // 解析请求头
            case CHECK_STATE_HEADER: {
                //printf("正在分析请求头...\n");
                ret = parse_headers(text);
                if(ret == BAD_REQUEST) { // 如果语法错误，直接返回
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) { // 如果是获取了一个完整请求（请求完成了）
                    return do_request(); // 解析具体的请求信息
                }
                break;
            }
            // 解析请求体
            case CHECK_STATE_CONTENT: {
                //printf("正在分析请求体...\n");
                ret = parse_content(text);
                if(ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN; // 如果解析失败了，将行状态改为行数据尚不完整
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    // 内部没有返回，就直接返回请求不完整
    return NO_REQUEST;
}

// 往写缓冲m_write_buf中写入待发送的数据
// 传入参数格式字符串和可变参数
bool Http_conn::add_response(const char* format, ...) {
    // 如果当前写缓冲区下标大于写缓冲区的最大（写缓冲区写满了）
    if( m_write_index >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    // 定义一个va_list可变参数列表
    va_list arg_list; // 这个主要是指向可变参数列表的
    va_start(arg_list, format); // 将可变参数列表指向可变参数起始位置
    // 通过vsnprintf函数，将格式化的数据写入写缓冲区，其类似sprintf
    int len = vsnprintf(m_write_buf + m_write_index, WRITE_BUFFER_SIZE - 1 - m_write_index, format, arg_list);
    // 如果len超过了可写入空间大小，写缓冲区就满了，就不继续写入了
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_index)) {
        return false;
    }
    // 每次写将增加
    m_write_index += len;
    // va_end宏结束对可变参数的访问
    va_end(arg_list);
    return true;
}

// 添加状态头
bool Http_conn::add_status_line( int status, const char* title ) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 添加请求头
bool Http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
    return true;
}

// 添加请求
bool Http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool Http_conn::add_linger() {
    return add_response("Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close");
}

bool Http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool Http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

bool Http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}


// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool Http_conn::process_write(HTTP_CODE ret) {
    switch(ret) {
        // 如果是内部错误
        case INTERNAL_ERROR:
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title );
            add_headers(m_file_stat.st_size);
            m_iv[ 0 ].iov_base = m_write_buf;
            m_iv[ 0 ].iov_len = m_write_index;
            m_iv[ 1 ].iov_base = m_file_address;
            m_iv[ 1 ].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        default:
            return false;
    }
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_index;
    m_iv_count = 1;
    return true;
}

// 由线程池的工作线程调用，这是处理HTTP请求的入口函数
void Http_conn::process() {
    // 这里要解析HTTP请求，然后生成响应
    // 1.解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST) {
        // 如果请求不完整
        modifyfd(m_epfd, m_sockfd, EPOLLIN); // 继续再获取该文件描述符的数据
        return;
    }

    // 2.生成响应(数据准备好写出去)
    //printf("开始生成响应...\n");
    bool write_ret = process_write(read_ret);
    if(!write_ret) {
        close_conn();
    }
    // 写成功后，将文件描述符写事件添加
    modifyfd(m_epfd, m_sockfd, EPOLLOUT);
}
