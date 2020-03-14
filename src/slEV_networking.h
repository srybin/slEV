#include <experimental/coroutine>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>

namespace slev {
    namespace io {
        namespace net {
            int kq;
    
            struct io_read_awaitable {
                char* buf;
                int nbytes;
                int socket;
            
	            int n;
	            std::experimental::coroutine_handle<> coro;
            
                constexpr bool await_ready() const noexcept { return false; }
                    
                void await_suspend(std::experimental::coroutine_handle<> c) {
                    coro = c;
                    struct kevent evSet;
                    EV_SET(&evSet, socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, this);
                    assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
                }
            
                auto await_resume() { return n; }
            }  
    
            struct io_write_awaitable {
            	int s;
            	char* buf;
            	int bytes;
            
                bool await_ready() noexcept { return false; }
                   
                void await_suspend(std::experimental::coroutine_handle<> coro) {
            		struct kevent evSet;
                    EV_SET(&evSet, s, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
                    assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
                }
               
                auto await_resume() { return write(s, buf, bytes); }
            }
    
            auto write_async(int socket, char* buf, int bytes) {
            	return io_write_awaitable {socket, buf, bytes};
            }
    
            auto read_async(const int socket, char* buf, const int nbytes) {
                return io_read_awaitable {buf, nbytes, socket};
            }

            namespace tcp {
                struct io_accept_awaitable {
                    int listening_socket;
	                struct sockaddr_storage addr;
	                socklen_t socklen = sizeof(addr);
	                bool success;
	                int err;
	                int s;
                   
                    bool await_ready() noexcept {
	                	s = accept(listening_socket, (struct sockaddr *)&addr, &socklen);
                        success = !(s == -1 && errno == EAGAIN);
                        err = errno;
                        return success;
                    }
                       
                    void await_suspend(std::experimental::coroutine_handle<> coro) {
	                	struct kevent evSet;
                        EV_SET(&evSet, listening_socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
                        assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
                    }
                   
                    auto await_resume() {
	                	if ( success ) {
	                		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
	                		return std::make_tuple(s, addr, err);
	                	}
                
	                	s = accept(listening_socket, (struct sockaddr *)&addr, &socklen);
                
	                	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
                
	                	return std::make_tuple(s, addr, errno);
                    }
                };
                
                struct io_connect_awaitable {
                	char* host;
                	int port;
                
                	int result;
                	int err;
                	int s;
                
                    bool await_ready() noexcept {
                		s = socket( AF_INET, SOCK_STREAM, 0 );
                		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
                		struct sockaddr_in peer;
                		peer.sin_family = AF_INET;
                        peer.sin_port = htons( port );
                		peer.sin_addr.s_addr = inet_addr( host );
                		result = connect( s, ( struct sockaddr * )&peer, sizeof( peer ) );
                		err = errno;
                		return result == 0 || (result = -1 && err != EINPROGRESS);
                    }
                       
                    void await_suspend(std::experimental::coroutine_handle<> coro) {
                		struct kevent evSet;
                        EV_SET(&evSet, s, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
                        assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
                    }
                   
                    auto await_resume() {
                		return std::make_tuple(s, result, err);
                    }
                };
                
                auto accept_async( const int listening_socket ) {
                    return io_accept_awaitable { listening_socket };
                }
                
                auto connect_async( char* host, int port) {
                	return io_connect_awaitable { host, port };
                }
                
                int listen(int port) {
                    struct sockaddr_in seraddr;
                    seraddr.sin_len = sizeof( seraddr );
                    seraddr.sin_family = AF_INET;
                    seraddr.sin_addr.s_addr = INADDR_ANY;
                    seraddr.sin_port = htons( port );
                    
                    int listening_socket = socket( seraddr.sin_family, SOCK_STREAM, 0 );
                    
                    int yes = 1;
                    setsockopt( listening_socket, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof( int ) );
                    fcntl( listening_socket, F_SETFL, fcntl( listening_socket, F_GETFL, 0) | O_NONBLOCK );
                    bind( listening_socket, ( struct sockaddr* ) &seraddr, sizeof( seraddr ) );
                    listen( listening_socket, SOMAXCONN );

                    struct kevent evSet;
                    EV_SET( &evSet, listening_socket, EVFILT_READ, EV_ADD, 0, 0, NULL );
                    assert( -1 != kevent( kq, &evSet, 1, NULL, 0, NULL ) );

                   return listening_socket; 
                }
            }
        }
    }
}