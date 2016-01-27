#include <zmq.h>
#include <assert.h>

void *zmq_ctx_new (void) {
    return NULL;
}

void *zmq_socket (void *, int type) {
    return NULL;
}

int zmq_close (void *s) {
    return (-1);
}

int zmq_setsockopt (void *s, int option, const void *optval,size_t optvallen) {
    return (-1);
}

int zmq_getsockopt (void *s, int option, void *optval,
                    size_t *optvallen) {
    return (-1);
}

int zmq_bind (void *s, const char *addr) {
    return (-1);
}

void *zmq_init (int io_threads) {
    return NULL;
}

int zmq_term (void *context) {
    return (-1);
}

int zmq_ctx_destroy (void *context) {
    return (-1);
}


int zmq_connect (void *s, const char *addr) {
    return (-1);
}

int zmq_send (void *s, const void *buf, size_t len, int flags) {
    return (-1);
}

int zmq_recv (void *s, void *buf, size_t len, int flags) {
    return (-1);
}

int zmq_errno (void) {
    return (-1);
}

const char *zmq_strerror (int errnum) {
    return "";
}

int zmq_msg_init (zmq_msg_t *msg) {
    return (-1);
}

int zmq_msg_recv (zmq_msg_t *msg, void *s, int flags) {
    return (-1);
}

int zmq_msg_close (zmq_msg_t *msg) {
    return (-1);
}

void *zmq_msg_data (zmq_msg_t *msg) {
    return NULL;
}

size_t zmq_msg_size (zmq_msg_t *msg) {
    return (0);
}

