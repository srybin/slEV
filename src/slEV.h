#include "./slEV_core.h"
#include "./slEV_networking.h"

namespace slev {
    void init() {
        silk::init_pool(slev::schedule, silk::makecontext, 1);

        slev::io::net::kq = kqueue();
    }

    int run() {
        struct kevent evSet;
        struct kevent evList[1024];

        while ( 1 ) {
            silk::join_main_thread_2_pool( slev::schedule );

            int nev = kevent( slev::io::net::kq, NULL, 0, evList, 1024, NULL );
    
            for (int i = 0; i < nev; i++) {
                silk::spawn( 0, (slev::frame*) evList[i].udata );
            }
        }
    }
}