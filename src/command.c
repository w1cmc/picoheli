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

// Put the buffer in BSS because there's not enough stack space.
static char buf[configSTATS_BUFFER_MAX_LENGTH];

static void run_date(const size_t argc, const char *argv[])
{
    const time_t now = time(NULL);
    const struct tm * const ptm = localtime(&now);
    assert(strftime(buf, sizeof(buf), "%c", ptm));
    puts(buf);
}

static void run_ps(const size_t argc, const char *argv[])
{
    vTaskList(buf);
    puts("Task          State  Priority  Stack        "
        "#\n************************************************");
    puts(buf);   
}

static void run_free(const size_t argc, const char *argv[])
{
    printf(
        "Configured total heap size:\t%d\n"
        "Free bytes in the heap now:\t%u\n"
        "Minimum number of unallocated bytes that have ever existed in the heap:\t%u\n",
        configTOTAL_HEAP_SIZE, xPortGetFreeHeapSize(),
        xPortGetMinimumEverFreeHeapSize());
}

static void run_top(const size_t argc, const char *argv[])
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
static void run_freqs(const size_t argc, const char *argv[])
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

static void run_set_sys_clock_48mhz(const size_t argc, const char *argv[])
{
    set_sys_clock_48mhz();
    setup_default_uart();
}

static void run_set_sys_clock_khz(const size_t argc, const char *argv[])
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

static void run_clr(const size_t argc, const char *argv[])
{
    const int gp = atoi(argv[1]);
    gpio_init(gp);
    gpio_set_dir(gp, GPIO_OUT);
    gpio_put(gp, 0);
}

static void run_set(const size_t argc, const char *argv[])
{
    const int gp = atoi(argv[1]);
    gpio_init(gp);
    gpio_set_dir(gp, GPIO_OUT);
    gpio_put(gp, 1);
}

static void run_ticks(const size_t argc, const char *argv[])
{
    printf("%lu ticks elapsed, %lu ms per tick\n", xTaskGetTickCount(), portTICK_PERIOD_MS);
}

static void run_help(const size_t argc, const char *argv[]);

static const struct {
    char const * const command;
    void (* const function)(const size_t argc, const char *argv[]);
    char const *const help;
    int min_argc, max_argc;
} cmds [] = {
    { "clr", run_clr, "Reset GPIO (turn it off)", 2, 2},
    { "date", run_date, "Print current date and time", 1, 1},
    { "free", run_free, "Print current heap stats", 1, 1},
    { "freqs", run_freqs, "Print system frequencies", 1, 1},
    { "help", run_help, "Shows this message", 1, 1},
    { "ps", run_ps, "Print current task stats", 1, 1},
    { "set", run_set, "Set GPIO (turn it on)", 2, 2},
    { "ticks", run_ticks, "Print current tick count", 1, 1},
    { "top", run_top, "Print time statistics", 1, 1},
    { 0 },
};

static const int cmd_table_last = sizeof(cmds)/sizeof(cmds[0]) - 1;

static void run_help(const size_t argc, const char *argv[])
{
    int i;

    for (i=0; i<cmd_table_last; ++i)
        printf("%s: %s\n", cmds[i].command, cmds[i].help);
}

static void parse_cmd(char *line)
{
    int argc = 1;
    const char ** argv = malloc(sizeof(char *));
    assert(argv);
    argv[0] = strtok(line, " ");
    if (!argv[0])
        argv[0] = line;

    int l = 0, m, r = cmd_table_last;
    int cmp;

    for (;;) {
        m = (l+r)/2;
        cmp = strcasecmp(argv[0], cmds[m].command);
        if (cmp < 0 && m < r)
            r = m;
        else if (0 < cmp && l < m)
            l = m;
        else
            break;
    }

    if (cmp == 0) {
        char * arg;
        while ((arg = strtok(NULL, " "))) {
            argv = realloc(argv, (argc + 1) * sizeof(char *));
            argv[argc] = arg;
            ++argc;
        }
        if (cmds[m].min_argc <= argc && argc <= cmds[m].max_argc)
            cmds[m].function(argc, argv);
        else
            puts("Wrong number of arguments");
    }
    else
        puts("Unrecognized command");

    free(argv);
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
            if (ptr < end)
                *ptr++ = c;
            break;
    }
}
