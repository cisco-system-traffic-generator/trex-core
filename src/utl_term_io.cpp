/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "utl_term_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <inttypes.h>
#include <fcntl.h>

static  struct termios oldterm;
struct termios orig_termios;

static void exit_handler1(void){
    tcsetattr(fileno(stdin), TCSANOW, &oldterm);
}

static void save_termio(void){
    tcgetattr(0, &oldterm);
}


void reset_terminal_mode(void){
    tcsetattr(0, TCSANOW, &orig_termios);
}

static void set_conio_terminal_mode(void){

    struct termios oldterm, term;

    tcgetattr(0, &oldterm);
    memcpy(&term, &oldterm, sizeof(term));
    term.c_lflag &= ~(ICANON | ECHO | ISIG);
    tcsetattr(0, TCSANOW, &term);
    setbuf(stdin, NULL);
}

static int kbhit(void) {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

static int getch(void){
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}


int utl_termio_init(){
    atexit(exit_handler1);
    save_termio();
    set_conio_terminal_mode();
    return (0);
}


int utl_termio_try_getch(void){
    if ( kbhit() ){
        return (getch());
    }else{
        return (0);
    }
}

int utl_termio_reset(void){
    tcsetattr(fileno(stdin), TCSANOW, &oldterm);
    return (0);
}


