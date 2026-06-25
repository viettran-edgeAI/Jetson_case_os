/* jetson_shield.c
 * Purpose: Jetson service that sends compact system stats to ESP32 over UART
 *          and answers bounded network/Wi-Fi control requests from the ESP32.
 * Compile: gcc -O2 -s -o jetson_shield jetson_shield.c
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define FIELD_SMALL 32
#define FIELD_MEDIUM 96
#define UART_LINE_MAX 256
#define CMD_LINE_MAX 256
#define RATE_MAX_KBPS 999999
#define SYSTEM_MONITOR_VERSION "1.5.1"

static volatile sig_atomic_t keep_running = 1;

static void debug_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[jetson_shield][debug] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

static void sig_handler(int signo)
{
    (void)signo;
    keep_running = 0;
}

struct stats_t {
    int has_any;
    int ram_pct;
    int cpu_pct;
    int gpu_pct;
    float cpu_temp;
    float gpu_temp;
    int power_mw;
    int power_mode_id;
    int power_mode_max_mw;
    char power_mode_name[FIELD_SMALL];
    int net_down_kbps;
    int net_up_kbps;
    int disk_read_kbps;
    int disk_write_kbps;
    int swap_used_mb;
    int swap_total_mb;
    int disk_used_pct;
    int disk_used_mb;
    int disk_total_mb;
};

struct net_sample_t {
    char iface[FIELD_SMALL];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
};

struct disk_sample_t {
    char device[FIELD_SMALL];
    uint64_t read_sectors;
    uint64_t write_sectors;
};

struct monitor_state_t {
    int has_net_prev;
    int has_disk_prev;
    double last_io_ts;
    struct net_sample_t net_prev;
    struct disk_sample_t disk_prev;
};

struct command_reader_t {
    char buf[CMD_LINE_MAX];
    size_t len;
};

static double monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static int clamp_rate(double value)
{
    if (value <= 0.0) return 0;
    if (value >= RATE_MAX_KBPS) return RATE_MAX_KBPS;
    return (int)(value + 0.5);
}

static int open_uart(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) return -1;

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static ssize_t write_all(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    const char *p = (const char *)buf;
    while (sent < len) {
        ssize_t n = write(fd, p + sent, len - sent);
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(1000);
            continue;
        }
        return -1;
    }
    return (ssize_t)sent;
}

static int uart_printf(int fd, const char *fmt, ...)
{
    char line[UART_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if ((size_t)n >= sizeof(line)) {
        line[sizeof(line) - 2] = '\n';
        line[sizeof(line) - 1] = '\0';
        n = (int)strlen(line);
    }

    char log_line[UART_LINE_MAX];
    strncpy(log_line, line, sizeof(log_line) - 1);
    log_line[sizeof(log_line) - 1] = '\0';
    log_line[strcspn(log_line, "\r\n")] = '\0';
    debug_log("UART TX: %s", log_line);

    return (write_all(fd, line, (size_t)n) < 0) ? -1 : 0;
}

static int is_probably_real_interface(const char *iface)
{
    return iface != NULL &&
           iface[0] != '\0' &&
           strcmp(iface, "lo") != 0 &&
           strncmp(iface, "docker", 6) != 0 &&
           strncmp(iface, "br-", 3) != 0 &&
           strncmp(iface, "veth", 4) != 0;
}

static int read_network_sample(struct net_sample_t *out)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return 0;

    char line[256];
    struct net_sample_t best = {{0}, 0, 0};
    uint64_t best_total = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char *name = line;
        while (isspace((unsigned char)*name)) ++name;
        char *end = colon - 1;
        while (end > name && isspace((unsigned char)*end)) *end-- = '\0';
        if (!is_probably_real_interface(name)) continue;

        unsigned long long rx = 0;
        unsigned long long tx = 0;
        int parsed = sscanf(colon + 1,
                            " %llu %*llu %*llu %*llu %*llu %*llu %*llu %*llu %llu",
                            &rx,
                            &tx);
        if (parsed != 2) continue;

        uint64_t total = (uint64_t)rx + (uint64_t)tx;
        if (total >= best_total) {
            best_total = total;
            strncpy(best.iface, name, sizeof(best.iface) - 1);
            best.rx_bytes = (uint64_t)rx;
            best.tx_bytes = (uint64_t)tx;
        }
    }

    fclose(fp);
    if (best.iface[0] == '\0') return 0;
    *out = best;
    return 1;
}

static int is_disk_candidate(const char *name)
{
    return strncmp(name, "nvme", 4) == 0 ||
           strncmp(name, "mmcblk", 6) == 0 ||
           strncmp(name, "sd", 2) == 0;
}

static int read_disk_sample(const char *preferred_device, struct disk_sample_t *out)
{
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) return 0;

    char line[256];
    struct disk_sample_t fallback = {{0}, 0, 0};
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        char name[FIELD_SMALL] = {0};
        unsigned long long read_sectors = 0;
        unsigned long long write_sectors = 0;

        int parsed = sscanf(line,
                            " %*u %*u %31s %*llu %*llu %llu %*llu %*llu %*llu %llu",
                            name,
                            &read_sectors,
                            &write_sectors);
        if (parsed != 3) continue;

        if (preferred_device != NULL &&
            preferred_device[0] != '\0' &&
            strcmp(name, preferred_device) == 0) {
            strncpy(out->device, name, sizeof(out->device) - 1);
            out->read_sectors = (uint64_t)read_sectors;
            out->write_sectors = (uint64_t)write_sectors;
            found = 1;
            break;
        }

        if (fallback.device[0] == '\0' && is_disk_candidate(name)) {
            strncpy(fallback.device, name, sizeof(fallback.device) - 1);
            fallback.read_sectors = (uint64_t)read_sectors;
            fallback.write_sectors = (uint64_t)write_sectors;
        }
    }

    fclose(fp);
    if (found) return 1;
    if (fallback.device[0] == '\0') return 0;
    *out = fallback;
    return 1;
}


static void power_mode_limit(int mode_id, const char *name, int *max_mw)
{
    int limit = -1;
    if (mode_id == 3) limit = 7000;
    else if (mode_id == 0) limit = 15000;
    else if (mode_id == 1) limit = 25000;
    else if (mode_id == 2) limit = 30000;
    if (name != NULL) {
        if (strstr(name, "7W") != NULL) limit = 7000;
        else if (strstr(name, "15W") != NULL) limit = 15000;
        else if (strstr(name, "25W") != NULL) limit = 25000;
        else if (strstr(name, "MAXN") != NULL || strstr(name, "SUPER") != NULL) limit = 30000;
    }
    if (max_mw != NULL) *max_mw = limit;
}

static int read_power_mode(int *mode_id, char *name, size_t name_size, int *max_mw)
{
    FILE *fp = popen("nvpmodel -q 2>/dev/null", "r");
    if (!fp) return 0;

    char line[128];
    int found = 0;
    int id = -1;
    char mode_name[FIELD_SMALL] = {0};
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *mode = strstr(line, "NV Power Mode:");
        if (mode != NULL) {
            mode += strlen("NV Power Mode:");
            while (*mode == ' ' || *mode == '\t') ++mode;
            snprintf(mode_name, sizeof(mode_name), "%s", mode);
            found = 1;
            continue;
        }
        int parsed_id = -1;
        if (sscanf(line, " %d", &parsed_id) == 1) {
            id = parsed_id;
            found = 1;
        }
    }
    (void)pclose(fp);
    if (!found) return 0;

    if (mode_name[0] == '\0') {
        if (id == 3) snprintf(mode_name, sizeof(mode_name), "7W");
        else if (id == 0) snprintf(mode_name, sizeof(mode_name), "15W");
        else if (id == 1) snprintf(mode_name, sizeof(mode_name), "25W");
        else if (id == 2) snprintf(mode_name, sizeof(mode_name), "MAXN_SUPER");
        else snprintf(mode_name, sizeof(mode_name), "UNKNOWN");
    }
    if (mode_id != NULL) *mode_id = id;
    if (name != NULL && name_size > 0) {
        snprintf(name, name_size, "%s", mode_name);
    }
    power_mode_limit(id, mode_name, max_mw);
    return 1;
}

static int set_power_mode_id(int mode_id)
{
    if (!(mode_id == 0 || mode_id == 1 || mode_id == 2 || mode_id == 3)) return 0;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "nvpmodel -m %d >/dev/null 2>&1", mode_id);
    int rc = system(cmd);
    return (rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0);
}

static int read_swap_usage_mb(int *used_mb, int *total_mb)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0;

    char key[FIELD_SMALL];
    unsigned long value = 0;
    char unit[FIELD_SMALL];
    unsigned long total = 0;
    unsigned long free_kb = 0;

    while (fscanf(fp, "%31s %lu %31s\n", key, &value, unit) == 3) {
        if (strcmp(key, "SwapTotal:") == 0) total = value;
        else if (strcmp(key, "SwapFree:") == 0) free_kb = value;
    }

    fclose(fp);
    if (free_kb > total) free_kb = total;
    if (used_mb != NULL) *used_mb = (int)(((total - free_kb) + 512UL) / 1024UL);
    if (total_mb != NULL) *total_mb = (int)((total + 512UL) / 1024UL);
    return 1;
}

static int read_disk_usage_mb(const char *mount_point, int *used_mb, int *total_mb, int *used_pct)
{
    struct statvfs st;
    if (statvfs(mount_point, &st) != 0 || st.f_blocks == 0) return 0;

    const unsigned long long total_bytes = (unsigned long long)st.f_blocks * (unsigned long long)st.f_frsize;
    const unsigned long long free_bytes = (unsigned long long)st.f_bfree * (unsigned long long)st.f_frsize;
    const unsigned long long used_bytes = (total_bytes > free_bytes) ? (total_bytes - free_bytes) : 0ULL;
    const unsigned long long mb = 1024ULL * 1024ULL;

    if (used_mb != NULL) *used_mb = (int)((used_bytes + (mb / 2ULL)) / mb);
    if (total_mb != NULL) *total_mb = (int)((total_bytes + (mb / 2ULL)) / mb);
    if (used_pct != NULL) {
        int pct = (int)(((double)used_bytes * 100.0 / (double)total_bytes) + 0.5);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        *used_pct = pct;
    }
    return 1;
}

static void update_io_metrics(struct monitor_state_t *state,
                              const char *disk_device,
                              struct stats_t *s)
{
    const double now = monotonic_seconds();
    double dt = 0.0;
    if (state->last_io_ts > 0.0 && now > state->last_io_ts) {
        dt = now - state->last_io_ts;
    }

    struct net_sample_t net = {{0}, 0, 0};
    if (read_network_sample(&net)) {
        if (state->has_net_prev &&
            strcmp(state->net_prev.iface, net.iface) == 0 &&
            dt > 0.0) {
            uint64_t rx_delta = (net.rx_bytes >= state->net_prev.rx_bytes)
                                    ? (net.rx_bytes - state->net_prev.rx_bytes)
                                    : 0;
            uint64_t tx_delta = (net.tx_bytes >= state->net_prev.tx_bytes)
                                    ? (net.tx_bytes - state->net_prev.tx_bytes)
                                    : 0;
            s->net_down_kbps = clamp_rate(((double)rx_delta / dt) / 1024.0);
            s->net_up_kbps = clamp_rate(((double)tx_delta / dt) / 1024.0);
        } else {
            s->net_down_kbps = 0;
            s->net_up_kbps = 0;
        }
        state->net_prev = net;
        state->has_net_prev = 1;
    }

    struct disk_sample_t disk = {{0}, 0, 0};
    if (read_disk_sample(disk_device, &disk)) {
        if (state->has_disk_prev &&
            strcmp(state->disk_prev.device, disk.device) == 0 &&
            dt > 0.0) {
            uint64_t read_delta = (disk.read_sectors >= state->disk_prev.read_sectors)
                                      ? (disk.read_sectors - state->disk_prev.read_sectors)
                                      : 0;
            uint64_t write_delta = (disk.write_sectors >= state->disk_prev.write_sectors)
                                       ? (disk.write_sectors - state->disk_prev.write_sectors)
                                       : 0;
            s->disk_read_kbps = clamp_rate((((double)read_delta * 512.0) / dt) / 1024.0);
            s->disk_write_kbps = clamp_rate((((double)write_delta * 512.0) / dt) / 1024.0);
        } else {
            s->disk_read_kbps = 0;
            s->disk_write_kbps = 0;
        }
        state->disk_prev = disk;
        state->has_disk_prev = 1;
    }

    if (!read_swap_usage_mb(&s->swap_used_mb, &s->swap_total_mb)) {
        s->swap_used_mb = -1;
        s->swap_total_mb = -1;
    }
    if (!read_disk_usage_mb("/", &s->disk_used_mb, &s->disk_total_mb, &s->disk_used_pct)) {
        s->disk_used_pct = -1;
        s->disk_used_mb = -1;
        s->disk_total_mb = -1;
    }
    if (!read_power_mode(&s->power_mode_id, s->power_mode_name, sizeof(s->power_mode_name), &s->power_mode_max_mw)) {
        s->power_mode_id = -1;
        s->power_mode_max_mw = -1;
        s->power_mode_name[0] = '\0';
    }
    state->last_io_ts = now;
}

static void init_stats(struct stats_t *s)
{
    memset(s, 0, sizeof(*s));
    s->ram_pct = -1;
    s->cpu_pct = -1;
    s->gpu_pct = -1;
    s->cpu_temp = -1.0f;
    s->gpu_temp = -1.0f;
    s->power_mw = -1;
    s->power_mode_id = -1;
    s->power_mode_max_mw = -1;
    s->power_mode_name[0] = '\0';
    s->net_down_kbps = -1;
    s->net_up_kbps = -1;
    s->disk_read_kbps = -1;
    s->disk_write_kbps = -1;
    s->swap_used_mb = -1;
    s->swap_total_mb = -1;
    s->disk_used_pct = -1;
    s->disk_used_mb = -1;
    s->disk_total_mb = -1;
}

static void parse_line(const char *raw, struct stats_t *s)
{
    init_stats(s);

    const char *p = strstr(raw, "RAM ");
    if (!p) return;
    s->has_any = 1;

    int used = 0;
    int total = 0;
    if (sscanf(p, "RAM %d/%dMB", &used, &total) == 2 && total > 0) {
        s->ram_pct = (int)((100.0 * used) / total + 0.5);
    }

    int sum = 0;
    int cnt = 0;
    const char *q = p;
    while ((q = strstr(q, "%@")) != NULL) {
        const char *r = q - 1;
        while (r >= p && (*r < '0' || *r > '9')) --r;
        if (r < p) {
            q += 2;
            continue;
        }
        const char *start = r;
        while (start > p && (*(start - 1) >= '0' && *(start - 1) <= '9')) --start;
        int val = 0;
        for (const char *t = start; t <= r; ++t) val = val * 10 + (*t - '0');
        sum += val;
        cnt++;
        q += 2;
    }
    if (cnt > 0) s->cpu_pct = (int)((float)sum / cnt + 0.5f);

    const char *gp = strstr(p, "GR3D_FREQ ");
    if (gp) {
        int g = -1;
        if (sscanf(gp, "GR3D_FREQ %d%%", &g) == 1) s->gpu_pct = g;
    }

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

    const char *pw = strstr(p, "VDD_IN ");
    if (pw) {
        int pm = -1;
        if (sscanf(pw, "VDD_IN %dmW", &pm) == 1) s->power_mw = pm;
    }
}

static void append_token(char *out, size_t outsz, int *pos, const char *fmt, ...)
{
    if (*pos >= (int)outsz - 2) return;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + *pos, outsz - (size_t)*pos, fmt, ap);
    va_end(ap);

    if (n < 0) return;
    if (*pos + n >= (int)outsz - 2) {
        out[outsz - 2] = '\n';
        out[outsz - 1] = '\0';
        *pos = (int)outsz - 2;
        return;
    }
    *pos += n;
}

static void format_msg(const struct stats_t *s, char *out, size_t outsz)
{
    int pos = 0;
    if (s->ram_pct >= 0) append_token(out, outsz, &pos, "RAM:%d ", s->ram_pct);
    if (s->cpu_pct >= 0) append_token(out, outsz, &pos, "CPU:%d ", s->cpu_pct);
    if (s->gpu_pct >= 0) append_token(out, outsz, &pos, "GPU:%d ", s->gpu_pct);
    if (s->cpu_temp >= 0.0f) append_token(out, outsz, &pos, "CT:%.1f ", s->cpu_temp);
    if (s->gpu_temp >= 0.0f) append_token(out, outsz, &pos, "GT:%.1f ", s->gpu_temp);
    if (s->power_mw >= 0) append_token(out, outsz, &pos, "P:%dmW ", s->power_mw);
    if (s->power_mode_id >= 0) append_token(out, outsz, &pos, "PM:%d ", s->power_mode_id);
    if (s->power_mode_max_mw > 0) append_token(out, outsz, &pos, "PMW:%d ", s->power_mode_max_mw);

    /* Upgrade tokens for the IO dashboard. Rates are KB/s, SW is used swap MB. */
    if (s->net_down_kbps >= 0) append_token(out, outsz, &pos, "ND:%d ", s->net_down_kbps);
    if (s->net_up_kbps >= 0) append_token(out, outsz, &pos, "NU:%d ", s->net_up_kbps);
    if (s->disk_read_kbps >= 0) append_token(out, outsz, &pos, "DR:%d ", s->disk_read_kbps);
    if (s->disk_write_kbps >= 0) append_token(out, outsz, &pos, "DW:%d ", s->disk_write_kbps);
    if (s->swap_used_mb >= 0) append_token(out, outsz, &pos, "SW:%d ", s->swap_used_mb);
    if (s->swap_total_mb >= 0) append_token(out, outsz, &pos, "ST:%d ", s->swap_total_mb);
    if (s->disk_used_pct >= 0) append_token(out, outsz, &pos, "DU:%d ", s->disk_used_pct);
    if (s->disk_used_mb >= 0) append_token(out, outsz, &pos, "DUM:%d ", s->disk_used_mb);
    if (s->disk_total_mb >= 0) append_token(out, outsz, &pos, "DTM:%d ", s->disk_total_mb);

    if (pos < (int)outsz - 2) {
        out[pos++] = '\n';
        out[pos] = '\0';
    } else {
        out[outsz - 2] = '\n';
        out[outsz - 1] = '\0';
    }
}

static void escape_field(const char *src, char *dst, size_t dstsz)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    if (dstsz == 0) return;

    for (size_t i = 0; src != NULL && src[i] != '\0' && pos + 1 < dstsz; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '_' || c == '-' || c == '.') {
            dst[pos++] = (char)c;
        } else {
            if (pos + 3 >= dstsz) break;
            dst[pos++] = '%';
            dst[pos++] = hex[c >> 4];
            dst[pos++] = hex[c & 0x0F];
        }
    }
    dst[pos] = '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void unescape_field(const char *src, char *dst, size_t dstsz)
{
    size_t pos = 0;
    if (dstsz == 0) return;

    for (size_t i = 0; src != NULL && src[i] != '\0' && pos + 1 < dstsz; ++i) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            dst[pos++] = (char)((hi << 4) | lo);
            i += 2;
        } else {
            dst[pos++] = src[i];
        }
    }
    dst[pos] = '\0';
}

static void unescape_nmcli_field(char *field)
{
    char *src = field;
    char *dst = field;
    while (*src != '\0') {
        if (*src == '\\' && src[1] != '\0') ++src;
        *dst++ = *src++;
    }
    *dst = '\0';
}

static int split_nmcli_escaped(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *start = line;
    int escaped = 0;

    for (char *p = line; ; ++p) {
        if (*p == '\0' || (*p == ':' && !escaped)) {
            if (count < max_fields) fields[count++] = start;
            if (*p == '\0') break;
            *p = '\0';
            start = p + 1;
            escaped = 0;
            continue;
        }
        if (*p == '\\' && !escaped) escaped = 1;
        else escaped = 0;
    }

    for (int i = 0; i < count; ++i) unescape_nmcli_field(fields[i]);
    return count;
}

static int get_interface_ipv4(const char *iface, char *addr, size_t addrsz)
{
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) return 0;

    int found = 0;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, iface) != 0) continue;

        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        if (inet_ntop(AF_INET, &sin->sin_addr, addr, (socklen_t)addrsz) != NULL) {
            found = 1;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return found;
}

static int get_active_ssid(char *ssid, size_t ssidsz)
{
    FILE *fp = popen("timeout 3s nmcli -t --escape yes -f ACTIVE,SSID dev wifi 2>/dev/null", "r");
    if (!fp) return 0;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[2] = {0};
        if (split_nmcli_escaped(line, fields, 2) != 2) continue;
        if (strcmp(fields[0], "yes") == 0) {
            strncpy(ssid, fields[1], ssidsz - 1);
            ssid[ssidsz - 1] = '\0';
            found = 1;
            break;
        }
    }
    pclose(fp);
    return found;
}

static void handle_ip_request(int uart)
{
    debug_log("REQ:IP received");
    struct net_sample_t net = {{0}, 0, 0};
    char ip[FIELD_SMALL] = "N/A";
    char ssid[FIELD_MEDIUM] = "";
    char esc_ssid[FIELD_MEDIUM] = "";
    char host[FIELD_SMALL] = "N/A";
    char esc_host[FIELD_MEDIUM] = "";
    const char *status = "DISCONNECTED";

    if (read_network_sample(&net) && get_interface_ipv4(net.iface, ip, sizeof(ip))) {
        status = "CONNECTED";
        (void)get_active_ssid(ssid, sizeof(ssid));
    } else {
        strncpy(net.iface, "N/A", sizeof(net.iface) - 1);
    }

    (void)gethostname(host, sizeof(host) - 1);
    escape_field(ssid, esc_ssid, sizeof(esc_ssid));
    escape_field(host, esc_host, sizeof(esc_host));
    debug_log("IP response iface=%s ssid=%s addr=%s status=%s host=%s", net.iface, esc_ssid[0] ? esc_ssid : "N/A", ip, status, esc_host[0] ? esc_host : "N/A");
    (void)uart_printf(uart, "IP IF:%s SSID:%s ADDR:%s STAT:%s HOST:%s\n",
                      net.iface,
                      esc_ssid[0] ? esc_ssid : "N/A",
                      ip,
                      status,
                      esc_host[0] ? esc_host : "N/A");
}

static void handle_wifi_scan(int uart)
{
    debug_log("REQ:WIFI_SCAN received");
    (void)uart_printf(uart, "WIFI_BEGIN\n");

    FILE *fp = popen("timeout 15s nmcli -t --escape yes -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list --rescan yes 2>/dev/null", "r");
    if (!fp) {
        (void)uart_printf(uart, "WIFI_ERR REASON:NMCLI_UNAVAILABLE\n");
        (void)uart_printf(uart, "WIFI_END COUNT:0\n");
        return;
    }

    char line[384];
    int count = 0;
    while (keep_running && fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[4] = {0};
        if (split_nmcli_escaped(line, fields, 4) < 4) continue;
        if (fields[1][0] == '\0') continue;

        char esc_ssid[72];
        char esc_sec[32];
        escape_field(fields[1], esc_ssid, sizeof(esc_ssid));
        escape_field(fields[3][0] ? fields[3] : "OPEN", esc_sec, sizeof(esc_sec));

        int signal_pct = atoi(fields[2]);
        if (signal_pct < 0) signal_pct = 0;
        if (signal_pct > 100) signal_pct = 100;
        int current = (strcmp(fields[0], "*") == 0) ? 1 : 0;

        debug_log("WIFI item ssid=%s sig=%d sec=%s cur=%d", esc_ssid, signal_pct, esc_sec, current);
        (void)uart_printf(uart, "WIFI SSID:%s SIG:%d SEC:%s CUR:%d\n",
                          esc_ssid,
                          signal_pct,
                          esc_sec,
                          current);
        if (++count >= 12) break;
    }

    int rc = pclose(fp);
    debug_log("WIFI scan done count=%d pclose_status=%d", count, rc);
    (void)uart_printf(uart, "WIFI_END COUNT:%d\n", count);
}

static int run_nmcli_connect(const char *ssid, const char *password)
{
    const int has_password = (password != NULL && password[0] != '\0');
    int pass_pipe[2] = {-1, -1};
    if (has_password && pipe(pass_pipe) != 0) return 0;

    pid_t pid = fork();
    if (pid < 0) {
        if (pass_pipe[0] >= 0) close(pass_pipe[0]);
        if (pass_pipe[1] >= 0) close(pass_pipe[1]);
        return 0;
    }

    if (pid == 0) {
        if (has_password) {
            close(pass_pipe[1]);
            dup2(pass_pipe[0], STDIN_FILENO);
            close(pass_pipe[0]);
        }

        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }

        if (has_password) {
            execlp("nmcli", "nmcli", "--ask", "dev", "wifi", "connect", ssid, (char *)NULL);
        } else {
            execlp("nmcli", "nmcli", "dev", "wifi", "connect", ssid, (char *)NULL);
        }
        _exit(127);
    }

    if (has_password) {
        close(pass_pipe[0]);
        (void)write_all(pass_pipe[1], password, strlen(password));
        (void)write_all(pass_pipe[1], "\n", 1);
        close(pass_pipe[1]);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR && keep_running) continue;
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void handle_wifi_connect(int uart, const char *cmd)
{
    const char *ssid_key = strstr(cmd, "SSID:");
    const char *psk_key = strstr(cmd, "PSK:");
    if (!ssid_key || !psk_key || psk_key <= ssid_key) {
        (void)uart_printf(uart, "WIFI_CONNECT FAIL REASON:BAD_REQUEST\n");
        return;
    }

    ssid_key += 5;
    psk_key += 4;

    char enc_ssid[FIELD_MEDIUM] = {0};
    char enc_psk[FIELD_MEDIUM] = {0};
    size_t ssid_len = 0;
    while (ssid_key[ssid_len] != '\0' &&
           !isspace((unsigned char)ssid_key[ssid_len]) &&
           ssid_len + 1 < sizeof(enc_ssid)) {
        enc_ssid[ssid_len] = ssid_key[ssid_len];
        ++ssid_len;
    }
    enc_ssid[ssid_len] = '\0';

    size_t psk_len = 0;
    while (psk_key[psk_len] != '\0' &&
           !isspace((unsigned char)psk_key[psk_len]) &&
           psk_len + 1 < sizeof(enc_psk)) {
        enc_psk[psk_len] = psk_key[psk_len];
        ++psk_len;
    }
    enc_psk[psk_len] = '\0';

    char ssid[FIELD_MEDIUM] = {0};
    char psk[FIELD_MEDIUM] = {0};
    unescape_field(enc_ssid, ssid, sizeof(ssid));
    unescape_field(enc_psk, psk, sizeof(psk));

    if (ssid[0] == '\0') {
        (void)uart_printf(uart, "WIFI_CONNECT FAIL REASON:EMPTY_SSID\n");
        return;
    }

    int ok = run_nmcli_connect(ssid, psk);
    char esc_ssid[72];
    escape_field(ssid, esc_ssid, sizeof(esc_ssid));
    (void)uart_printf(uart, "WIFI_CONNECT %s SSID:%s\n", ok ? "OK" : "FAIL", esc_ssid);
}


static const char *env_default(const char *name, const char *fallback)
{
    const char *v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int valid_unit_name(const char *name)
{
    if (name == NULL || name[0] == '\0') return 0;
    for (const char *p = name; *p != '\0'; ++p) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.' || c == '@')) return 0;
    }
    return 1;
}

static int run_systemctl_unit(const char *verb, const char *unit)
{
    if (!valid_unit_name(unit)) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        execlp("systemctl", "systemctl", verb, unit, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR && keep_running) continue;
        return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int run_systemctl_unit_async(const char *verb, const char *unit)
{
    if (!valid_unit_name(unit)) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        execlp("systemctl", "systemctl", verb, unit, (char *)NULL);
        _exit(127);
    }
    return 1;
}

static int run_systemctl_no_unit(const char *verb)
{
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        execlp("systemctl", "systemctl", verb, (char *)NULL);
        _exit(127);
    }
    return 1;
}

static int command_output_first_line(const char *cmd, char *out, size_t outsz)
{
    if (out == NULL || outsz == 0) return 0;
    out[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    int ok = 0;
    if (fgets(out, outsz, fp)) {
        out[strcspn(out, "\r\n")] = '\0';
        ok = 1;
    }
    pclose(fp);
    return ok;
}

static void service_status_values(const char *unit, char *stat, size_t statsz, char *enabled, size_t ensz)
{
    snprintf(stat, statsz, "UNKNOWN");
    snprintf(enabled, ensz, "N/A");
    if (!valid_unit_name(unit)) return;

    char cmd[FIELD_MEDIUM];
    char line[FIELD_SMALL];
    snprintf(cmd, sizeof(cmd), "systemctl is-active %s 2>/dev/null", unit);
    if (command_output_first_line(cmd, line, sizeof(line))) {
        if (strcmp(line, "active") == 0) snprintf(stat, statsz, "RUNNING");
        else if (strcmp(line, "inactive") == 0 || strcmp(line, "failed") == 0) snprintf(stat, statsz, "STOPPED");
        else snprintf(stat, statsz, "UNKNOWN");
    }

    snprintf(cmd, sizeof(cmd), "systemctl is-enabled %s 2>/dev/null", unit);
    if (command_output_first_line(cmd, line, sizeof(line))) {
        snprintf(enabled, ensz, "%s", (strcmp(line, "enabled") == 0) ? "1" : "0");
    }
}

static void handle_about_request(int uart)
{
    char host[FIELD_SMALL] = "N/A";
    char esc_host[FIELD_SMALL] = "N/A";
    char ip[FIELD_SMALL] = "N/A";
    struct net_sample_t net = {{0}, 0, 0};
    (void)gethostname(host, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    escape_field(host, esc_host, sizeof(esc_host));
    if (read_network_sample(&net)) (void)get_interface_ipv4(net.iface, ip, sizeof(ip));
    (void)uart_printf(uart, "ABOUT HOST:%s IP:%s VER:%s\n", esc_host, ip, SYSTEM_MONITOR_VERSION);
}

static void handle_service_status(int uart, int ngrok)
{
    const char *env_name = ngrok ? "SYSTEM_MONITOR_NGROK_SERVICE" : "SYSTEM_MONITOR_SSH_SERVICE";
    const char *fallback = ngrok ? "ngrok-ssh.service" : "ssh.service";
    const char *unit = env_default(env_name, fallback);
    char stat[16];
    char enabled[8];
    char esc_unit[24];
    service_status_values(unit, stat, sizeof(stat), enabled, sizeof(enabled));
    escape_field(unit, esc_unit, sizeof(esc_unit));

    if (!ngrok) {
        (void)uart_printf(uart, "SSH STAT:%s EN:%s SVC:%s\n", stat, enabled, esc_unit);
        return;
    }

    char endpoint[49] = "N/A";
    char api[16] = "UNAVAILABLE";
    char json[512];
    const char *api_url = env_default("SYSTEM_MONITOR_NGROK_API", "http://127.0.0.1:4040/api/tunnels");
    char curl_cmd[256];
    snprintf(curl_cmd, sizeof(curl_cmd), "timeout 2s curl -fsS '%s' 2>/dev/null", api_url);
    if (command_output_first_line(curl_cmd, json, sizeof(json))) {
        char *tcp = strstr(json, "tcp://");
        if (tcp) {
            tcp += 6;
            size_t n = 0;
            while (tcp[n] != '\0' && tcp[n] != '"' && n + 1 < sizeof(endpoint)) {
                endpoint[n] = tcp[n];
                ++n;
            }
            endpoint[n] = '\0';
            snprintf(api, sizeof(api), "%s", endpoint[0] ? "OK" : "NO_TCP");
        } else {
            snprintf(api, sizeof(api), "NO_TCP");
        }
    }
    char esc_endpoint[49];
    escape_field(endpoint, esc_endpoint, sizeof(esc_endpoint));
    (void)uart_printf(uart, "NGROK STAT:%s EN:%s END:%s API:%s SVC:%s\n", stat, enabled, esc_endpoint, api, esc_unit);
}

static void handle_service_action(int uart, int ngrok, const char *action)
{
    const char *unit = env_default(ngrok ? "SYSTEM_MONITOR_NGROK_SERVICE" : "SYSTEM_MONITOR_SSH_SERVICE",
                                   ngrok ? "ngrok-ssh.service" : "ssh.service");
    int ok = run_systemctl_unit(action, unit);
    (void)uart_printf(uart, "%s_RESULT ACTION:%s STAT:%s\n",
                      ngrok ? "NGROK" : "SSH",
                      strcmp(action, "start") == 0 ? "START" : "STOP",
                      ok ? "OK" : "FAIL");
}

static void headless_status_values(char *def, size_t defsz, char *act, size_t actsz)
{
    char line[FIELD_SMALL];
    snprintf(def, defsz, "UNKNOWN");
    snprintf(act, actsz, "UNKNOWN");
    if (command_output_first_line("systemctl get-default 2>/dev/null", line, sizeof(line))) {
        if (strcmp(line, "multi-user.target") == 0) snprintf(def, defsz, "HEADLESS");
        else if (strcmp(line, "graphical.target") == 0) snprintf(def, defsz, "GRAPHICAL");
    }
    if (command_output_first_line("systemctl is-active graphical.target 2>/dev/null", line, sizeof(line))) {
        if (strcmp(line, "active") == 0) snprintf(act, actsz, "GRAPHICAL");
        else snprintf(act, actsz, "HEADLESS");
    }
}

static void handle_headless_status(int uart)
{
    char def[16];
    char act[16];
    headless_status_values(def, sizeof(def), act, sizeof(act));
    (void)uart_printf(uart, "HEADLESS DEF:%s ACT:%s\n", def, act);
}

static void handle_headless_action(int uart, const char *action)
{
    int ok = 0;
    const char *target = "UNKNOWN";
    if (strcmp(action, "ENABLE_BOOT") == 0) {
        target = "multi-user.target";
        ok = run_systemctl_unit("set-default", target);
    } else if (strcmp(action, "DISABLE_BOOT") == 0) {
        target = "graphical.target";
        ok = run_systemctl_unit("set-default", target);
    } else if (strcmp(action, "APPLY_NOW") == 0) {
        char def[16];
        char act[16];
        headless_status_values(def, sizeof(def), act, sizeof(act));
        target = (strcmp(def, "HEADLESS") == 0) ? "multi-user.target" : "graphical.target";
        ok = run_systemctl_unit("isolate", target);
    }
    char esc_target[FIELD_SMALL];
    escape_field(target, esc_target, sizeof(esc_target));
    (void)uart_printf(uart, "HEADLESS_RESULT ACTION:%s STAT:%s TARGET:%s\n", action, ok ? "OK" : "FAIL", esc_target);
}


static void handle_power_mode_status(int uart)
{
    int id = -1;
    int max_mw = -1;
    char name[FIELD_SMALL] = {0};
    char esc_name[FIELD_SMALL];
    if (!read_power_mode(&id, name, sizeof(name), &max_mw)) {
        snprintf(name, sizeof(name), "UNKNOWN");
    }
    escape_field(name, esc_name, sizeof(esc_name));
    (void)uart_printf(uart, "POWER_MODE ID:%d NAME:%s MAX:%d\n", id, esc_name, max_mw);
}

static void handle_power_mode_set(int uart, int id)
{
    int ok = set_power_mode_id(id);
    int current_id = -1;
    int max_mw = -1;
    char name[FIELD_SMALL] = {0};
    char esc_name[FIELD_SMALL];
    (void)read_power_mode(&current_id, name, sizeof(name), &max_mw);
    if (name[0] == '\0') snprintf(name, sizeof(name), "UNKNOWN");
    escape_field(name, esc_name, sizeof(esc_name));
    (void)uart_printf(uart, "POWER_MODE_RESULT ACTION:SET STAT:%s ID:%d NAME:%s MAX:%d\n", ok ? "OK" : "FAIL", current_id, esc_name, max_mw);
}

static void handle_system_action(int uart, const char *action)
{
    if (strcmp(action, "MONITOR_RESTART") == 0) {
        const char *unit = env_default("SYSTEM_MONITOR_SELF_SERVICE", "jetson_shield.service");
        (void)uart_printf(uart, "SYSTEM_RESULT ACTION:RESTART_MONITOR STAT:ACK\n");
        (void)run_systemctl_unit_async("restart", unit);
        return;
    }
    if (strcmp(action, "JETSON_REBOOT") == 0) {
        (void)uart_printf(uart, "SYSTEM_RESULT ACTION:REBOOT STAT:ACK\n");
        (void)run_systemctl_no_unit("reboot");
        return;
    }
    if (strcmp(action, "JETSON_SHUTDOWN") == 0) {
        (void)uart_printf(uart, "SYSTEM_RESULT ACTION:SHUTDOWN STAT:ACK\n");
        (void)run_systemctl_no_unit("poweroff");
        return;
    }
    (void)uart_printf(uart, "SYSTEM_RESULT ACTION:%s STAT:FAIL\n", action);
}

static void handle_command_line(int uart, const char *cmd)
{
    if (strcmp(cmd, "REQ:IP") == 0 || strcmp(cmd, "IP?") == 0) {
        handle_ip_request(uart);
    } else if (strcmp(cmd, "REQ:WIFI_SCAN") == 0 || strcmp(cmd, "WIFI_SCAN") == 0) {
        handle_wifi_scan(uart);
    } else if (strncmp(cmd, "WIFI_CONNECT ", 13) == 0) {
        handle_wifi_connect(uart, cmd + 13);
    } else if (strcmp(cmd, "REQ:ABOUT") == 0) {
        handle_about_request(uart);
    } else if (strcmp(cmd, "REQ:SSH_STATUS") == 0) {
        handle_service_status(uart, 0);
    } else if (strcmp(cmd, "REQ:NGROK_STATUS") == 0) {
        handle_service_status(uart, 1);
    } else if (strcmp(cmd, "SSH_START") == 0) {
        handle_service_action(uart, 0, "start");
    } else if (strcmp(cmd, "SSH_STOP") == 0) {
        handle_service_action(uart, 0, "stop");
    } else if (strcmp(cmd, "NGROK_START") == 0) {
        handle_service_action(uart, 1, "start");
    } else if (strcmp(cmd, "NGROK_STOP") == 0) {
        handle_service_action(uart, 1, "stop");
    } else if (strcmp(cmd, "REQ:HEADLESS_STATUS") == 0) {
        handle_headless_status(uart);
    } else if (strcmp(cmd, "HEADLESS_ENABLE_BOOT") == 0) {
        handle_headless_action(uart, "ENABLE_BOOT");
    } else if (strcmp(cmd, "HEADLESS_DISABLE_BOOT") == 0) {
        handle_headless_action(uart, "DISABLE_BOOT");
    } else if (strcmp(cmd, "HEADLESS_APPLY_NOW") == 0) {
        handle_headless_action(uart, "APPLY_NOW");
    } else if (strcmp(cmd, "REQ:POWER_MODE") == 0) {
        handle_power_mode_status(uart);
    } else if (strncmp(cmd, "POWER_MODE_SET ", 15) == 0) {
        handle_power_mode_set(uart, atoi(cmd + 15));
    } else if (strcmp(cmd, "MONITOR_RESTART") == 0 ||
               strcmp(cmd, "JETSON_REBOOT") == 0 ||
               strcmp(cmd, "JETSON_SHUTDOWN") == 0) {
        handle_system_action(uart, cmd);
    } else if (cmd[0] != '\0') {
        (void)uart_printf(uart, "ERR UNKNOWN_CMD\n");
    }
}

static int poll_uart_commands(int uart, struct command_reader_t *reader)
{
    char tmp[96];
    int result = 0;

    for (;;) {
        ssize_t n = read(uart, tmp, sizeof(tmp));
        if (n > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                char c = tmp[i];
                if (c == '\r') continue;
                if (c == '\n') {
                    reader->buf[reader->len] = '\0';
                    debug_log("UART RX CMD: %s", reader->buf);
                    handle_command_line(uart, reader->buf);
                    reader->len = 0;
                    result = 1;
                } else if (reader->len + 1 < sizeof(reader->buf)) {
                    reader->buf[reader->len++] = c;
                } else {
                    reader->len = 0;
                    (void)uart_printf(uart, "ERR CMD_TOO_LONG\n");
                }
            }
            continue;
        }

        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n == 0) break;
        return -1;
    }

    return result;
}

int main(int argc, char **argv)
{
    const char *uart_path = getenv("SYSTEM_MONITOR_UART");
    const char *disk_device = getenv("SYSTEM_MONITOR_DISK");
    const char *cmd = getenv("SYSTEM_MONITOR_TEGRASTATS_CMD");

    if (uart_path == NULL || uart_path[0] == '\0') uart_path = "/dev/ttyTHS1";
    if (disk_device == NULL || disk_device[0] == '\0') disk_device = "nvme0n1";
    if (cmd == NULL || cmd[0] == '\0') cmd = "tegrastats --interval 1000 2>&1";
    if (argc > 1) uart_path = argv[1];

    debug_log("starting uart=%s disk=%s cmd=%s", uart_path, disk_device, cmd);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        perror("popen tegrastats");
        return 1;
    }

    int uart = -1;
    for (int i = 0; i < 5 && uart < 0 && keep_running; ++i) {
        uart = open_uart(uart_path);
        if (uart < 0) {
            debug_log("open_uart failed attempt=%d path=%s errno=%d", i + 1, uart_path, errno);
            sleep(1);
        } else {
            debug_log("open_uart ok path=%s fd=%d", uart_path, uart);
        }
    }

    struct monitor_state_t monitor_state;
    memset(&monitor_state, 0, sizeof(monitor_state));
    struct command_reader_t command_reader;
    memset(&command_reader, 0, sizeof(command_reader));

    char line[1024];
    char out[UART_LINE_MAX];
    while (keep_running && fgets(line, sizeof(line), pipe)) {
        struct stats_t s;
        parse_line(line, &s);
        if (!s.has_any) {
            if (uart >= 0 && poll_uart_commands(uart, &command_reader) < 0) {
                close(uart);
                uart = -1;
            }
            continue;
        }

        update_io_metrics(&monitor_state, disk_device, &s);
        format_msg(&s, out, sizeof(out));

        if (uart < 0) {
            for (int b = 0; b < 10 && uart < 0 && keep_running; ++b) {
                uart = open_uart(uart_path);
                if (uart < 0) sleep(1);
            }
            if (uart < 0) continue;
        }

        if (poll_uart_commands(uart, &command_reader) < 0 ||
            write_all(uart, out, strlen(out)) < 0) {
            close(uart);
            uart = -1;
        }

        usleep(10000);
    }

    if (uart >= 0) close(uart);
    pclose(pipe);
    return 0;
}
