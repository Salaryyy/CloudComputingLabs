#ifndef _EPOLL_SERVER_H
#define _EPOLL_SERVER_H

int init_listen_fd(int port, int epfd);
void epoll_run(int port, int n_thread);
void do_accept(int lfd, int epfd);
void do_read(int cfd, int epfd);
int get_line(int sock, char *buf, int size);
void disconnect(int cfd, int epfd);
void http_request(const char* request, int cfd);
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len);
void send_file(int cfd, const char* filename);
void send_dir(int cfd, const char* dirname);
void encode_str(char* to, int tosize, const char* from);
void decode_str(char *to, char *from);
const char *get_file_type(const char *name);

#endif
