/* system_monitor.c
 * Purpose: lightweight tegrastats Jetson service to send Jetson system stats to ESP32 over UART
 * Compile: gcc -O2 -s -o system_monitor_minimal system_monitor.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t keep_running = 1;
void sig_handler(int signo) { (void)signo; keep_running = 0; }

struct stats_t {
    int has_any;
    int ram_pct;    /* percent */
    int cpu_pct;    /* average percent */
    int gpu_pct;
    float cpu_temp;
    float gpu_temp;
    int power_mw;
};

static int open_uart(const char *path)
{
    int fd = open(path, O_WRONLY | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;       /* 8 bits */
    tty.c_cflag &= ~PARENB;   /* no parity */
    tty.c_cflag &= ~CSTOPB;   /* 1 stop bit */
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5; /* 0.5s read timeout if reading */

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* write all bytes */
static ssize_t write_all(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    const char *p = (const char*)buf;
    while (sent < len) {
        ssize_t n = write(fd, p + sent, len - sent);
        if (n > 0) sent += (size_t)n;
        else {
            if (errno == EINTR) continue;
            return -1;
        }
    }
    return (ssize_t)sent;
}

/* parse line without regex */
static void parse_line(const char *raw, struct stats_t *s)
{
    s->has_any = 0;
    s->ram_pct = s->cpu_pct = s->gpu_pct = -1;
    s->cpu_temp = s->gpu_temp = -1.0f;
    s->power_mw = -1;

    const char *p = strstr(raw, "RAM ");
    if (!p) return;
    s->has_any = 1;

    /* RAM x/yMB */
    int used=0, total=0;
    if (sscanf(p, "RAM %d/%dMB", &used, &total) == 2 && total > 0) {
        s->ram_pct = (int)((100.0 * used) / total + 0.5);
    }

    /* CPU: find all occurrences of number%@ and average */
    int sum = 0, cnt = 0;
    const char *q = p;
    while ((q = strstr(q, "%@")) != NULL) {
        const char *r = q - 1;
        /* skip non-digit */
        while (r >= p && (*r < '0' || *r > '9')) --r;
        if (r < p) { q += 2; continue; }
        const char *start = r;
        while (start > p && (*(start-1) >= '0' && *(start-1) <= '9')) --start;
        int val = 0;
        for (const char *t = start; t <= r; ++t) val = val*10 + (*t - '0');
        sum += val; cnt++;
        q += 2;
    }
    if (cnt > 0) s->cpu_pct = (int)((float)sum / cnt + 0.5f);

    /* GPU: GR3D_FREQ N% */
    const char *gp = strstr(p, "GR3D_FREQ ");
    if (gp) {
        int g = -1;
        if (sscanf(gp, "GR3D_FREQ %d%%", &g) == 1) s->gpu_pct = g;
    }

    /* cpu@temp and gpu@temp */
    const char *ct = strstr(p, "cpu@");
    if (ct) {
        float tmp = 0;
        if (sscanf(ct, "cpu@%fC", &tmp) == 1) s->cpu_temp = tmp;
    }
    const char *gt = strstr(p, "gpu@");
    if (gt) {
        float tmp = 0;
        if (sscanf(gt, "gpu@%fC", &tmp) == 1) s->gpu_temp = tmp;
    }

    /* Power: VDD_IN N mW */
    const char *pw = strstr(p, "VDD_IN ");
    if (pw) {
        int pm = -1;
        if (sscanf(pw, "VDD_IN %dmW", &pm) == 1) s->power_mw = pm;
    }
}

/* format into small message */
static void format_msg(const struct stats_t *s, char *out, size_t outsz)
{
    int pos = 0;
    if (s->ram_pct >= 0) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "RAM:%d ", s->ram_pct);
    if (s->cpu_pct >= 0) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "CPU:%d ", s->cpu_pct);
    if (s->gpu_pct >= 0) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "GPU:%d ", s->gpu_pct);
    if (s->cpu_temp >= 0.0f) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "CT:%.1f ", s->cpu_temp);
    if (s->gpu_temp >= 0.0f) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "GT:%.1f ", s->gpu_temp);
    if (s->power_mw >= 0) pos += snprintf(out + pos, (outsz>pos? outsz-pos:0), "P:%dmW ", s->power_mw);
    if (pos < (int)outsz - 2) { out[pos++] = '\n'; out[pos] = '\0'; }
    else out[outsz-1] = '\0';
}

int main(int argc, char **argv)
{
    const char *uart_path = "/dev/ttyTHS1";
    const char *cmd = "tegrastats --interval 1000 2>&1";

    if (argc > 1) uart_path = argv[1]; /* allow override */

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        perror("popen tegrastats");
        return 1;
    }

    int uart = -1;
    /* attempt to open UART a few times */
    for (int i=0; i<5 && uart<0 && keep_running; ++i) {
        uart = open_uart(uart_path);
        if (uart < 0) sleep(1);
    }

    char line[1024];
    char out[128];
    while (keep_running && fgets(line, sizeof(line), pipe)) {
        struct stats_t s;
        parse_line(line, &s);
        if (!s.has_any) continue;
        format_msg(&s, out, sizeof(out));
        if (uart < 0) {
            /* try reopen with backoff */
            for (int b=0; b<10 && uart<0 && keep_running; ++b) {
                uart = open_uart(uart_path);
                if (uart < 0) sleep(1);
            }
            if (uart < 0) continue;
        }
        if (write_all(uart, out, strlen(out)) < 0) {
            /* write failed: close and mark for reopen */
            close(uart);
            uart = -1;
        }
        /* slight sleep to avoid bursts */
        usleep(10000); /* 10ms */
    }

    if (uart >= 0) close(uart);
    pclose(pipe);
    return 0;
}