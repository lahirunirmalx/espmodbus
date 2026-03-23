/**
 * Serial port implementation for Linux - thread-safe.
 */

#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <pthread.h>

struct serial_port {
    int fd;
    struct termios old_termios;
    pthread_mutex_t write_lock;
    char read_buf[512];
    size_t read_pos;
};

static speed_t baud_to_speed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B115200;
    }
}

serial_port_t *serial_open(const char *device, int baud) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "serial_open: cannot open %s: %s\n", device, strerror(errno));
        return NULL;
    }

    serial_port_t *sp = calloc(1, sizeof(*sp));
    if (!sp) {
        close(fd);
        return NULL;
    }
    sp->fd = fd;
    sp->read_pos = 0;
    pthread_mutex_init(&sp->write_lock, NULL);

    if (tcgetattr(fd, &sp->old_termios) < 0) {
        fprintf(stderr, "serial_open: tcgetattr failed: %s\n", strerror(errno));
        close(fd);
        free(sp);
        return NULL;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1; /* 100ms timeout */

    speed_t sp_baud = baud_to_speed(baud);
    cfsetispeed(&tio, sp_baud);
    cfsetospeed(&tio, sp_baud);

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        fprintf(stderr, "serial_open: tcsetattr failed: %s\n", strerror(errno));
        close(fd);
        free(sp);
        return NULL;
    }

    return sp;
}

void serial_close(serial_port_t *sp) {
    if (!sp)
        return;
    tcsetattr(sp->fd, TCSANOW, &sp->old_termios);
    close(sp->fd);
    pthread_mutex_destroy(&sp->write_lock);
    free(sp);
}

bool serial_is_open(serial_port_t *sp) {
    return sp && sp->fd >= 0;
}

int serial_get_fd(serial_port_t *sp) {
    return sp ? sp->fd : -1;
}

bool serial_send_line(serial_port_t *sp, const char *line) {
    if (!sp || sp->fd < 0 || !line)
        return false;

    pthread_mutex_lock(&sp->write_lock);

    size_t len = strlen(line);
    ssize_t written = write(sp->fd, line, len);
    if (written != (ssize_t)len) {
        pthread_mutex_unlock(&sp->write_lock);
        return false;
    }
    written = write(sp->fd, "\n", 1);
    tcdrain(sp->fd);

    pthread_mutex_unlock(&sp->write_lock);
    return written == 1;
}

bool serial_read_line(serial_port_t *sp, char *buf, size_t buflen, int timeout_ms) {
    if (!sp || sp->fd < 0 || !buf || buflen == 0)
        return false;

    buf[0] = '\0';

    /* Check if we already have a complete line in buffer */
    for (size_t i = 0; i < sp->read_pos; i++) {
        if (sp->read_buf[i] == '\n') {
            /* Copy up to newline */
            size_t copy_len = (i < buflen - 1) ? i : buflen - 1;
            memcpy(buf, sp->read_buf, copy_len);
            buf[copy_len] = '\0';

            /* Remove \r if present */
            if (copy_len > 0 && buf[copy_len - 1] == '\r')
                buf[copy_len - 1] = '\0';

            /* Shift buffer */
            size_t remaining = sp->read_pos - i - 1;
            if (remaining > 0)
                memmove(sp->read_buf, sp->read_buf + i + 1, remaining);
            sp->read_pos = remaining;

            return true;
        }
    }

    /* Need more data */
    fd_set rfds;
    struct timeval tv;

    int total_waited = 0;
    int wait_chunk = 10; /* 10ms chunks */

    while (total_waited < timeout_ms || timeout_ms < 0) {
        FD_ZERO(&rfds);
        FD_SET(sp->fd, &rfds);

        if (timeout_ms < 0) {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
        } else {
            int remaining = timeout_ms - total_waited;
            if (remaining > wait_chunk) remaining = wait_chunk;
            tv.tv_sec = remaining / 1000;
            tv.tv_usec = (remaining % 1000) * 1000;
        }

        int ret = select(sp->fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return false;
        }

        if (ret > 0 && FD_ISSET(sp->fd, &rfds)) {
            /* Read available data */
            size_t space = sizeof(sp->read_buf) - sp->read_pos - 1;
            if (space > 0) {
                ssize_t n = read(sp->fd, sp->read_buf + sp->read_pos, space);
                if (n > 0) {
                    sp->read_pos += n;
                    sp->read_buf[sp->read_pos] = '\0';

                    /* Check for complete line */
                    for (size_t i = 0; i < sp->read_pos; i++) {
                        if (sp->read_buf[i] == '\n') {
                            size_t copy_len = (i < buflen - 1) ? i : buflen - 1;
                            memcpy(buf, sp->read_buf, copy_len);
                            buf[copy_len] = '\0';

                            if (copy_len > 0 && buf[copy_len - 1] == '\r')
                                buf[copy_len - 1] = '\0';

                            size_t remaining = sp->read_pos - i - 1;
                            if (remaining > 0)
                                memmove(sp->read_buf, sp->read_buf + i + 1, remaining);
                            sp->read_pos = remaining;

                            return true;
                        }
                    }
                }
            }
        }

        total_waited += wait_chunk;
        if (timeout_ms == 0) break; /* Non-blocking mode */
    }

    return false;
}

bool serial_command(serial_port_t *sp, const char *cmd, char *resp, size_t resp_len, int timeout_ms) {
    if (!serial_send_line(sp, cmd))
        return false;
    return serial_read_line(sp, resp, resp_len, timeout_ms);
}

void serial_flush(serial_port_t *sp) {
    if (!sp || sp->fd < 0)
        return;
    tcflush(sp->fd, TCIFLUSH);
    sp->read_pos = 0;
}
