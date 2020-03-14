#include "./../src/slEV.h"

slev::independed_task process_connection(const int s) {
    char buf[1024];
    int n;
         
    while (1) {
        n = co_await slev::io::net::read_async(s, buf, 1024);

        if (n <= 0) {
            printf("process_connection(%d) has been disconnected...\n", s);
            close(s);

            co_return;
        }

        printf("process_connection(%d) [%d] %s\n", s, n, buf);
    }
}

int main() {
    slev::init();

    auto log_new_connection = []( int s, struct sockaddr_storage addr ) -> slev::task<> {
        co_await slev::yield();
        
        char ip[NI_MAXHOST];
        char port[NI_MAXSERV];
         
        getnameinfo(
            (struct sockaddr *)&addr,
            sizeof(addr),
            ip,
            sizeof(ip),
            port,
            sizeof(port),
            NI_NUMERICHOST | NI_NUMERICSERV
            );
        
        printf( "New connection: %s:%s, %d...\n", ip, port, s );

        co_return;
    };

    auto server = [&]( int listening_socket ) -> slev::independed_task {
        while ( 1 ) {
            auto[ s, addr, err ] = co_await slev::io::net::tcp::accept_async( listening_socket );
            
            if ( s ) {
                process_connection( s );
           
                co_await log_new_connection( s, addr );
            }
        }
    };

    auto client = [&] () -> slev::independed_task {
        auto[ s, result, err ] = co_await slev::io::net::tcp::connect_async( "127.0.0.1", 3491 );

        char* m = "C3333333333333333333333333333333333333333333333";

        for ( int i = 0; i < 100000; i++ ) {
            co_await slev::io::net::write_async( s, m, 48 );
        }

        close( s );
    };

    server( slev::io::net::tcp::listen( 3491 ) );

    client();

    client();

    return slev::run();
}