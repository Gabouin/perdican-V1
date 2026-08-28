#include "board.h"
#include "console.h"
#include "usb_cdc.h"
#include "lsm6ds3.h"
#include "i2c.h"
#include "gpio.h"
#include "systick.h"
#include "dfu.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#define LINE_MAX    96
#define ARG_MAX     8

static char     s_line[LINE_MAX];
static uint16_t s_len;
static bool     s_prompt_pending;

static void prompt(void)
{
    cdc_puts("perdican> ");
    cdc_flush();
}

void console_banner(void)
{
    const uint32_t *uid = (const uint32_t *)UID_BASE;

    cdc_puts("\r\n");
    cdc_puts("  ____  _____ ____  ____ ___ ____    _    _   _\r\n");
    cdc_puts(" |  _ \\| ____|  _ \\|  _ \\_ _/ ___|  / \\  | \\ | |\r\n");
    cdc_puts(" | |_) |  _| | |_) | | | | | |     / _ \\ |  \\| |\r\n");
    cdc_puts(" |  __/| |___|  _ <| |_| | | |___ / ___ \\| |\\  |\r\n");
    cdc_puts(" |_|   |_____|_| \\_\\____/___\\____/_/   \\_\\_| \\_|  V1\r\n");
    cdc_puts("\r\n");

    printf(" STM32G431CBT6 @ %lu MHz   LSM6DS3TR-C @ 0x%02X\r\n",
           (unsigned long)(BOARD_SYSCLK_HZ / 1000000u), IMU_I2C_ADDR7);
    printf(" UID %08lX%08lX%08lX   %u KB flash\r\n",
           (unsigned long)uid[2], (unsigned long)uid[1], (unsigned long)uid[0],
           (unsigned)*(const uint16_t *)FLASHSIZE_BASE);
    cdc_puts(" type 'help' for commands\r\n\r\n");
    cdc_flush();
}

static void cmd_help(void)
{
    cdc_puts(
        "commands:\r\n"
        "  help                 this list\r\n"
        "  id                   board, clock and chip identity\r\n"
        "  uptime               milliseconds since boot\r\n"
        "\r\n"
        "  led on|off|toggle    drive the user LED (PA3)\r\n"
        "  btn                  read the user button (PA2)\r\n"
        "  vbus                 USB bus power present (PA9)\r\n"
        "\r\n"
        "  imu                  one accel/gyro/temp reading\r\n"
        "  imu stream [hz]      stream readings, any key stops\r\n"
        "  imu selftest         run the datasheet self-test\r\n"
        "  imu regs             dump the key control registers\r\n"
        "\r\n"
        "  i2cscan              probe every address on I2C1\r\n"
        "\r\n"
        "  pins                 expansion header map\r\n"
        "  gpio <pin> read      read a header pin (e.g. gpio PB0 read)\r\n"
        "  gpio <pin> out 0|1   drive a header pin\r\n"
        "  gpio <pin> in [pu|pd]  make a header pin an input\r\n"
        "\r\n"
        "  reset                reboot the board\r\n"
        "  dfu                  reboot into the USB DFU bootloader\r\n");
}

static void cmd_id(void)
{
    const uint32_t *uid = (const uint32_t *)UID_BASE;

    printf("board    : PERDICAN V1\r\n");
    printf("mcu      : STM32G431CBT6  (LQFP48, 128 KB flash, 32 KB SRAM)\r\n");
    printf("uid      : %08lX%08lX%08lX\r\n",
           (unsigned long)uid[2], (unsigned long)uid[1], (unsigned long)uid[0]);
    printf("flash    : %u KB\r\n", (unsigned)*(const uint16_t *)FLASHSIZE_BASE);
    printf("sysclk   : %lu Hz (HSI16 -> PLL, no crystal fitted)\r\n",
           (unsigned long)BOARD_SYSCLK_HZ);
    printf("usb clk  : HSI48 + CRS locked to host SOF\r\n");
    printf("i2c1     : PB6/PB7 @ 400 kHz, clocked from HSI16\r\n");
    printf("imu      : LSM6DS3TR-C at 7-bit 0x%02X (SA0 grounded)\r\n", IMU_I2C_ADDR7);
    printf("vbus     : %s\r\n", vbus_present() ? "present" : "absent");
}

static void print_sample(const lsm6_sample_t *s)
{
    printf("accel  x %+8.3f  y %+8.3f  z %+8.3f  g\r\n",
           (double)s->accel_g[0], (double)s->accel_g[1], (double)s->accel_g[2]);
    printf("gyro   x %+8.2f  y %+8.2f  z %+8.2f  dps\r\n",
           (double)s->gyro_dps[0], (double)s->gyro_dps[1], (double)s->gyro_dps[2]);
    printf("temp   %.2f C\r\n", (double)s->temp_c);
}

static void cmd_imu(int argc, char **argv)
{
    lsm6_sample_t s;

    if (argc >= 2 && strcasecmp(argv[1], "stream") == 0) {
        uint32_t hz = (argc >= 3) ? (uint32_t)strtoul(argv[2], 0, 10) : 20u;
        if (hz == 0u)   hz = 1u;
        if (hz > 200u)  hz = 200u;

        const uint32_t period = 1000u / hz;
        printf("streaming at %lu Hz, press any key to stop\r\n", (unsigned long)hz);

        uint32_t next = millis();
        for (;;) {
            if (cdc_available()) {
                while (cdc_available())
                    (void)cdc_getc();
                cdc_puts("stopped\r\n");
                return;
            }

            if (!timer_elapsed(&next, period))
                continue;

            if (lsm6ds3_read(&s) != I2C_OK) {
                cdc_puts("imu read failed\r\n");
                return;
            }

            printf("a %+7.3f %+7.3f %+7.3f g   g %+8.2f %+8.2f %+8.2f dps   %.1f C\r\n",
                   (double)s.accel_g[0], (double)s.accel_g[1], (double)s.accel_g[2],
                   (double)s.gyro_dps[0], (double)s.gyro_dps[1], (double)s.gyro_dps[2],
                   (double)s.temp_c);
            cdc_flush();
        }
    }

    if (argc >= 2 && strcasecmp(argv[1], "selftest") == 0) {
        bool accel_ok = false, gyro_ok = false;
        cdc_puts("running self-test (about 1 s, keep the board still)...\r\n");
        cdc_flush();

        i2c_status_t st = lsm6ds3_self_test(&accel_ok, &gyro_ok);
        if (st != I2C_OK) {
            printf("self-test aborted: %s\r\n", i2c_strerror(st));
            return;
        }
        printf("accelerometer : %s\r\n", accel_ok ? "PASS" : "FAIL");
        printf("gyroscope     : %s\r\n", gyro_ok  ? "PASS" : "FAIL");
        return;
    }

    if (argc >= 2 && strcasecmp(argv[1], "regs") == 0) {
        static const struct { uint8_t reg; const char *name; } regs[] = {
            { LSM6_WHO_AM_I,   "WHO_AM_I  " },
            { LSM6_CTRL1_XL,   "CTRL1_XL  " },
            { LSM6_CTRL2_G,    "CTRL2_G   " },
            { LSM6_CTRL3_C,    "CTRL3_C   " },
            { LSM6_CTRL4_C,    "CTRL4_C   " },
            { LSM6_CTRL6_C,    "CTRL6_C   " },
            { LSM6_CTRL7_G,    "CTRL7_G   " },
            { LSM6_STATUS_REG, "STATUS_REG" },
        };
        for (unsigned i = 0; i < sizeof regs / sizeof regs[0]; i++) {
            uint8_t v = 0;
            i2c_status_t st = i2c_read_reg(IMU_I2C_ADDR7, regs[i].reg, &v);
            if (st == I2C_OK)
                printf("  0x%02X %s = 0x%02X\r\n", regs[i].reg, regs[i].name, v);
            else
                printf("  0x%02X %s : %s\r\n", regs[i].reg, regs[i].name, i2c_strerror(st));
        }
        printf("  accel sensitivity %.3f mg/LSB, gyro %.3f mdps/LSB\r\n",
               (double)lsm6ds3_accel_sensitivity_mg(),
               (double)lsm6ds3_gyro_sensitivity_mdps());
        return;
    }

    i2c_status_t st = lsm6ds3_read(&s);
    if (st != I2C_OK) {
        printf("imu read failed: %s\r\n", i2c_strerror(st));
        return;
    }
    print_sample(&s);
    printf("raw    a %6d %6d %6d   g %6d %6d %6d\r\n",
           s.raw_accel[0], s.raw_accel[1], s.raw_accel[2],
           s.raw_gyro[0],  s.raw_gyro[1],  s.raw_gyro[2]);
}

static void cmd_i2cscan(void)
{
    cdc_puts("scanning I2C1 (PB6/PB7)...\r\n");

    unsigned found = 0;
    for (uint8_t a = 0x08u; a <= 0x77u; a++) {
        if (!i2c_probe(a))
            continue;

        found++;
        if (a == IMU_I2C_ADDR7)
            printf("  0x%02X  LSM6DS3TR-C (on-board IMU)\r\n", a);
        else
            printf("  0x%02X\r\n", a);
        cdc_flush();
    }

    printf("%u device%s found\r\n", found, found == 1u ? "" : "s");
}

static void cmd_pins(void)
{
    cdc_puts("J1 (left)              J2 (right)\r\n");
    cdc_puts("  1  GND                 1  GND\r\n");
    cdc_puts("  2  +3V3                2  +3V3\r\n");

    for (unsigned i = 0; i < 7u; i++) {
        const board_gpio_t *l = &board_gpios[i];
        const board_gpio_t *r = &board_gpios[i + 7u];
        const char *note = (r->pin == SWDIO_PIN) ? " / SWDIO"
                         : (r->pin == SWCLK_PIN) ? " / SWCLK" : "";
        printf("  %u  %-4s                 %u  %s%s\r\n",
               l->position, l->name, r->position, r->name, note);
    }

    cdc_puts("\r\non-board: PA3 LED (active high), PA2 button (active low),\r\n");
    cdc_puts("          PB6/PB7 IMU I2C, PA0/PA1 IMU INT1/INT2,\r\n");
    cdc_puts("          PA9 VBUS sense, PB8 BOOT0, PA11/PA12 USB\r\n");
    cdc_puts("DEBUG hdr: 1 +3V3, 2 SWDIO, 3 SWCLK, 4 GND\r\n");
}

static void cmd_gpio(int argc, char **argv)
{
    if (argc < 3) {
        cdc_puts("usage: gpio <pin> read | out 0|1 | in [pu|pd]\r\n");
        return;
    }

    const board_gpio_t *g = board_gpio_find(argv[1]);
    if (!g) {
        printf("'%s' is not an expansion pin on this board; try 'pins'\r\n", argv[1]);
        return;
    }

    const bool is_swd  = board_pin_is_swd(g->port, g->pin);
    const bool forced  = (argc >= 4 && strcasecmp(argv[argc - 1], "force") == 0)
                      || (argc >= 5 && strcasecmp(argv[argc - 1], "force") == 0);

    if (is_swd && strcasecmp(argv[2], "read") != 0 && !forced) {
        printf("%s is also %s on the DEBUG header.\r\n", g->name,
               g->pin == SWDIO_PIN ? "SWDIO" : "SWCLK");
        cdc_puts("Reconfiguring it will drop an attached debugger.\r\n");
        cdc_puts("Append 'force' if you really mean it.\r\n");
        return;
    }

    if (strcasecmp(argv[2], "read") == 0) {
        printf("%s = %u\r\n", g->name, gpio_read(g->port, g->pin) ? 1u : 0u);
        return;
    }

    if (strcasecmp(argv[2], "out") == 0) {
        if (argc < 4) {
            cdc_puts("usage: gpio <pin> out 0|1\r\n");
            return;
        }
        bool high = (argv[3][0] == '1');
        gpio_write(g->port, g->pin, high);
        gpio_config_output(g->port, g->pin, GPIO_PP, GPIO_SPEED_LOW);
        gpio_write(g->port, g->pin, high);
        printf("%s driven %s\r\n", g->name, high ? "high" : "low");
        return;
    }

    if (strcasecmp(argv[2], "in") == 0) {
        gpio_pull_t pull = GPIO_PULL_NONE;
        const char *what = "floating";

        if (argc >= 4 && strcasecmp(argv[3], "pu") == 0) {
            pull = GPIO_PULL_UP;   what = "pull-up";
        } else if (argc >= 4 && strcasecmp(argv[3], "pd") == 0) {
            pull = GPIO_PULL_DOWN; what = "pull-down";
        }

        gpio_config_input(g->port, g->pin, pull);
        printf("%s is now an input (%s), reads %u\r\n",
               g->name, what, gpio_read(g->port, g->pin) ? 1u : 0u);
        return;
    }

    cdc_puts("usage: gpio <pin> read | out 0|1 | in [pu|pd]\r\n");
}

static void cmd_led(int argc, char **argv)
{
    if (argc < 2) {
        printf("led is %s\r\n", led_get() ? "on" : "off");
        return;
    }

    if (strcasecmp(argv[1], "on") == 0)          led_set(true);
    else if (strcasecmp(argv[1], "off") == 0)    led_set(false);
    else if (strcasecmp(argv[1], "toggle") == 0) led_toggle();
    else {
        cdc_puts("usage: led on|off|toggle\r\n");
        return;
    }

    printf("led is %s\r\n", led_get() ? "on" : "off");
}

static void run(char *line)
{
    char *argv[ARG_MAX];
    int   argc = 0;

    for (char *p = line; *p && argc < ARG_MAX; ) {
        while (*p == ' ' || *p == '\t')
            *p++ = '\0';
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }

    if (argc == 0)
        return;

    if (strcasecmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0)
        cmd_help();
    else if (strcasecmp(argv[0], "id") == 0)
        cmd_id();
    else if (strcasecmp(argv[0], "uptime") == 0)
        printf("%lu ms\r\n", (unsigned long)millis());
    else if (strcasecmp(argv[0], "led") == 0)
        cmd_led(argc, argv);
    else if (strcasecmp(argv[0], "btn") == 0)
        printf("button %s\r\n", button_pressed() ? "PRESSED" : "released");
    else if (strcasecmp(argv[0], "vbus") == 0)
        printf("vbus %s\r\n", vbus_present() ? "present" : "absent");
    else if (strcasecmp(argv[0], "imu") == 0)
        cmd_imu(argc, argv);
    else if (strcasecmp(argv[0], "i2cscan") == 0)
        cmd_i2cscan();
    else if (strcasecmp(argv[0], "pins") == 0)
        cmd_pins();
    else if (strcasecmp(argv[0], "gpio") == 0)
        cmd_gpio(argc, argv);
    else if (strcasecmp(argv[0], "reset") == 0) {
        cdc_puts("resetting\r\n");
        cdc_flush();
        delay_ms(50);
        NVIC_SystemReset();
    }
    else if (strcasecmp(argv[0], "dfu") == 0) {
        cdc_puts("rebooting into DFU; flash with:\r\n");
        cdc_puts("  dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D perdican.bin\r\n");
        cdc_flush();
        delay_ms(100);
        dfu_reboot_to_bootloader();
    }
    else
        printf("unknown command '%s'; try 'help'\r\n", argv[0]);
}

void console_init(void)
{
    s_len = 0;
    s_prompt_pending = true;
}

void console_poll(void)
{
    if (!cdc_is_connected()) {
        s_len = 0;
        s_prompt_pending = true;
        return;
    }

    if (s_prompt_pending) {
        s_prompt_pending = false;
        prompt();
    }

    int c;
    while ((c = cdc_getc()) >= 0) {

        if (c == '\r' || c == '\n') {
            cdc_puts("\r\n");
            s_line[s_len] = '\0';
            run(s_line);
            s_len = 0;
            prompt();
            continue;
        }

        if (c == 0x08 || c == 0x7F) {
            if (s_len) {
                s_len--;
                cdc_puts("\b \b");
            }
            continue;
        }

        if (c == 0x03) {
            cdc_puts("^C\r\n");
            s_len = 0;
            prompt();
            continue;
        }

        if (c < 0x20 || c > 0x7E)
            continue;

        if (s_len < LINE_MAX - 1u) {
            const char ch = (char)c;
            s_line[s_len++] = ch;
            cdc_write(&ch, 1u);
        }
    }

    cdc_flush();
}
