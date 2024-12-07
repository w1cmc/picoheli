#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
//
#include <FreeRTOS.h>
#include <task.h>
//
#include <hardware/clocks.h>
#include <hardware/rtc.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
//
#include "command.h"
#include "onewire.h"
#include "fourway.h"
#include "blheli.h"

// Put the buffer in BSS because there's not enough stack space.
#if configSTATS_BUFFER_MAX_LENGTH < 0xFFFFU
static char buf[configSTATS_BUFFER_MAX_LENGTH];
#else
static char buf[1024];
#endif

static void run_date(int argc, const char *argv[])
{
    const time_t now = time(NULL);
    const struct tm * const ptm = localtime(&now);
    assert(strftime(buf, sizeof(buf), "%c", ptm));
    puts(buf);
}

static void run_ps(int argc, const char *argv[])
{
    vTaskList(buf);
    puts("Task          State  Priority  Stack        "
        "#\n************************************************");
    puts(buf);   
}

static void run_free(int argc, const char *argv[])
{
    printf(
        "Configured total heap size:\t%d\n"
        "Free bytes in the heap now:\t%u\n"
        "Minimum number of unallocated bytes that have ever existed in the heap:\t%u\n",
        configTOTAL_HEAP_SIZE, xPortGetFreeHeapSize(),
        xPortGetMinimumEverFreeHeapSize());
}

static void run_top(int argc, const char *argv[])
{
    /* A buffer into which the execution times will be
     * written, in ASCII form.  This buffer is assumed to be large enough to
     * contain the generated report.  Approximately 40 bytes per task should
     * be sufficient.
     */
    printf("%s",
           "Task            Abs Time      % Time\n"
           "****************************************\n");
    /* Generate a table of task stats. */
    vTaskGetRunTimeStats(buf);
    printf("%s\n", buf);
}

/* Derived from pico-examples/clocks/hello_48MHz/hello_48MHz.c */
static void run_freqs(int argc, const char *argv[])
{   
    uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    printf("pll_usb  = %dkHz\n", f_pll_usb);
    printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\treported  = %lukHz\n", f_clk_sys, clock_get_hz(clk_sys) / KHZ);
    printf("clk_peri = %dkHz\treported  = %lukHz\n", f_clk_peri, clock_get_hz(clk_peri) / KHZ);
    printf("clk_usb  = %dkHz\treported  = %lukHz\n", f_clk_usb, clock_get_hz(clk_usb) / KHZ);
    printf("clk_adc  = %dkHz\treported  = %lukHz\n", f_clk_adc, clock_get_hz(clk_adc) / KHZ);
    printf("clk_rtc  = %dkHz\treported  = %lukHz\n", f_clk_rtc, clock_get_hz(clk_rtc) / KHZ);

    // Can't measure clk_ref / xosc as it is the ref
}

static void run_set_sys_clock_48mhz(int argc, const char *argv[])
{
    set_sys_clock_48mhz();
    setup_default_uart();
}

static void run_set_sys_clock_khz(int argc, const char *argv[])
{
    const int khz = atoi(argv[0]);
    bool configured = set_sys_clock_khz(khz, false);
    if (!configured) {
        printf("Not possible. Clock not configured.\n");
        return;
    }
    /*
    By default, when reconfiguring the system clock PLL settings after runtime initialization,
    the peripheral clock is switched to the 48MHz USB clock to ensure continuity of peripheral operation.
    There seems to be a problem with running the SPI 2.4 times faster than the system clock,
    even at the same SPI baud rate.
    Anyway, for now, reconfiguring the peripheral clock to the system clock at its new frequency works OK.
    */
    bool ok = clock_configure(clk_peri,
                              0,
                              CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                              clock_get_hz(clk_sys),
                              clock_get_hz(clk_sys));
    assert(ok);

    setup_default_uart();
}

static void run_gpio(int argc, const char *argv[]) {
    char *end = 0;
    const unsigned int gp = strtoul(argv[1], &end, 0);
    if (*argv[1] && !*end && 0 <= gp && gp <= 28) {
        const int val = !strcmp(argv[0], "set");
        gpio_init(gp);
        gpio_set_dir(gp, GPIO_OUT);
        gpio_put(gp, val);
    }
    else {
        puts("error: expecting a number between 0 and 28");
    }
}

static void run_pull(int argc, const char *argv[])
{
    char * end = 0;
    const unsigned int gp = strtoul(argv[1], &end, 0);
    if (*argv[1] && !*end && 0 <= gp && gp <= 28) {
        const int up = !strcmp(argv[0], "pup");
        gpio_set_dir(gp, GPIO_IN);
        if (up)
            gpio_pull_up(gp);
        else
            gpio_pull_down(gp);
    }
    else {
        puts("error: expecting a number between 0 and 28");
    }
}

static int flip(int gp)
{
    static int up = 0;
    if (up)
        gpio_pull_up(ONEWIRE_PIN);
    else
        gpio_pull_down(ONEWIRE_PIN);
    up = !up;
}

static void run_pulse(int argc, const char *argv[])
{
    const int nt = argc - 1;
    const size_t tsz = sizeof(int) * nt;
    int * const t = alloca(tsz);
    int *tp = t;

    memset(t, 0, tsz);
    for (--argc, ++argv; argc; --argc, ++argv) {
        char * end = 0;
        *tp++ = strtoul(*argv, &end, 0);
        if (!**argv || *end) {
            puts("error: expecting a number of ms");
            return;
        }
    }

    for (tp = t; tp < &t[nt]; ++tp) {
        flip(ONEWIRE_PIN);
        vTaskDelay(pdMS_TO_TICKS(*tp));
    }
    flip(ONEWIRE_PIN);
}

static void run_ticks(int argc, const char *argv[])
{
    printf("%lu ticks elapsed, %lu ms per tick\n", xTaskGetTickCount(), portTICK_PERIOD_MS);
}

static void run_onewire(const char * tx_buf, uint tx_size)
{
    static char rx_buf[256];
    static const uint rx_size = count_of(rx_buf);
    const uint n = onewire_xfer(tx_buf, tx_size, rx_buf, rx_size);
    if (n == 0) {
        puts("no data received");
        return;
    }

    int i;
    for (i=0; i<n; ++i) {
        printf("%02x ", rx_buf[i]);
    }
    putchar('\n');
}

static void run_blheli(int argc, const char *argv[])
{
    static pkt_t pkt = {
        .start = 0x2f,
        .cmd = cmd_DeviceInitFlash,
        .param_len = 1,
        .param = {0}, // XXX: ESC number from argv?
    };

    if (blheli_DeviceInitFlash(&pkt) == ACK_OK)
        puts("OK");
    else
        puts("Error");
}

static void run_ping(int argc, const char *argv[])
{
    if (blheli_ping() == ACK_OK)
        puts("OK");
    else
        puts("Error");
}

static void run_addr(int argc, const char *argv[])
{
    char *end = 0;
    uint16_t addr = strtoul(argv[1], &end, 0);
    if (!*argv[1] || *end) {
        puts("error: expecting an address between 0 and 65535");
        return;
    }

    const uint8_t be_addr[] = { addr >> 8, addr & 255 };
    if (blheli_set_addr(be_addr) == ACK_OK)
        puts("OK");
    else
        puts("Error");
}

static void run_read(int argc, const char *argv[])
{
    char *end = 0;
    if (argc == 3) {
        run_addr(argc, argv);
        --argc;
        ++argv;
    }

    const uint rx_size = strtoul(argv[1], &end, 0);
    if (!*argv[1] || *end || rx_size < 1 || 256 < rx_size) {
        puts("error: expecting a number of bytes between 1 and 256");
        return;
    }

    char * const rx_buf = malloc(rx_size); // no check: CRT panics on OOM
    if (blheli_read_flash(rx_buf, rx_size) == ACK_OK)
        puts("OK");
    else
        puts("Error");
    free(rx_buf);
}

static void run_restart(int argc, const char *argv[])
{
    if (blheli_DeviceReset(NULL) == ACK_OK)
        puts("OK");
    else
        puts("Error");
}

static void run_break(int argc, const char *argv[])
{
    onewire_break();
}

static void run_help(int argc, const char *argv[]);

typedef struct {
    const char * command;
    void (* const function)(int argc, const char *argv[]);
    const char * help;
    int min_argc, max_argc;
} cmd_ent_t;

static const cmd_ent_t cmds [] = {
    { "clr", run_gpio, "Reset GPIO (turn it off)", 2, 2},
    { "set", run_gpio, "Set GPIO (turn it on)", 2, 2},
    { "pup", run_pull, "Configure pull-up on GPIO", 2, 2},
    { "pdn", run_pull, "Configure pull-down on GPIO", 2, 2},
    { "date", run_date, "Print current system date and time", 1, 1},
    { "free", run_free, "Print current heap stats", 1, 1},
    { "ps", run_ps, "Print current task stats", 1, 1},
    { "top", run_top, "Print task time statistics", 1, 1},
    { "ticks", run_ticks, "Print current tick count", 1, 1},
    { "freqs", run_freqs, "Print system frequencies", 1, 1},
    { "blheli", run_blheli, "Send BLHeli handshake to ESC", 1, 1},
    { "ping", run_ping, "Send keep alive to ESC", 1, 1},
    { "addr", run_addr, "Set the read/buffer address in ESC", 2, 2 },
    { "read", run_read, "Read n bytes from the address set above", 2, 3 },
    { "break", run_break, "Send a break to the ESC", 1, 1},
    { "restart", run_restart, "Send restart command to ESC", 1, 1},
    { "pulse", run_pulse, "Pulse the 1-wire pin for some ms", 1, 10},
    { "help", run_help, "Shows this message", 1, 1},
};

#define ncmds (sizeof(cmds)/sizeof(cmds[0]))
static const cmd_ent_t * sorted [ncmds + 1];

static int compar(const void *a, const void *b)
{
    return strcmp((*(const cmd_ent_t **) a)->command, (*(const cmd_ent_t **) b)->command);
}

static void __attribute__((constructor)) command_init()
{
    static const cmd_ent_t last = {0};
    int i;

    for (i=0; i<ncmds; ++i)
        sorted[i] = &cmds[i];
    sorted[ncmds] = &last;
    qsort(sorted, ncmds, sizeof(sorted[0]), compar);
}

static const cmd_ent_t * command_find(const char *command)
{
    int l = 0, m, r = ncmds + 1;
    int cmp;

    for (;;) {
        m = (l+r)/2;
        cmp = strcasecmp(command, sorted[m]->command);
        if (cmp < 0 && m < r)
            r = m;
        else if (0 < cmp && l < m)
            l = m;
        else
            break;
    }

    if (cmp == 0)
        return sorted[m];
    
    return NULL;
}

static void run_help(int argc, const char *argv[])
{
    int i;

    for (i=0; i<ncmds; ++i)
        printf("%s: %s\n", cmds[i].command, cmds[i].help);
}

static void parse_cmd(char *line)
{
    while (*line && isspace(*line))
        ++line;
    if (!*line)
        goto prompt;

    int argc = 1;
    const char ** argv = malloc(sizeof(char *));
    assert(argv);
    argv[0] = strtok(line, " ");
    if (!argv[0])
        argv[0] = line;

    const cmd_ent_t * const cmd = command_find(argv[0]);
    if (cmd) {
        char * arg;
        while ((arg = strtok(NULL, " "))) {
            argv = realloc(argv, (argc + 1) * sizeof(char *));
            argv[argc] = arg;
            ++argc;
        }
        if (cmd->min_argc <= argc && argc <= cmd->max_argc)
            cmd->function(argc, argv);
        else
            puts("Wrong number of arguments");
    }
    else
        puts("Unrecognized command");

    free(argv);
prompt:
    fputs("CLI> ", stdout);
    fflush(stdout);
}

void input_scan(int c)
{
    static char line[80], *ptr = line;
    static char * const end = &line[sizeof(line)];
    static const char backup[] = "\033[1D\033[K";
    const char *p;

    switch (c) {
        case '\010': /* backspace */
        case '\177': /* delete */
            if (ptr > line) {
                --ptr;
                for (p = backup; *p; ++p)
                    putchar(*p);
            }
            break;
        case '\n':
        case '\r':
            putchar('\r');
            putchar('\n');
            *ptr = 0;
            ptr = line;
            parse_cmd(line);
            break;
        default:
            putchar(c);
            if (ptr < &end[-1])
                *ptr++ = c;
            break;
    }
}
