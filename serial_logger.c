/*
 * Cross-platform serial logger
 *
 * Ubuntu 22.04:
 *     gcc -O2 -Wall -Wextra -o serial_logger serial_logger.c
 *     ./serial_logger -port /dev/ttyUSB1
 *
 * Windows (MinGW):
 *     gcc -O2 -Wall -Wextra -o serial_logger.exe serial_logger.c
 *     serial_logger.exe -port COM3
 *
 * Serial settings:
 *     Baud: 115000
 *     Data: 8 bits
 *     Parity: None
 *     Stop: 1
 *
 * Expected data:
 *
 *     T:1125.00 S:0 I:4.59 V:22.66 L:0.00
 *
 * Everything else is ignored.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#ifdef _WIN32

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

#else

    #include <errno.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/time.h>

#endif


/* ============================================================
 * Global state
 * ============================================================ */

static volatile int running = 1;

#ifdef _WIN32
static HANDLE serial_handle = INVALID_HANDLE_VALUE;
#else
static int serial_fd = -1;
#endif


/* ============================================================
 * Signal handling
 * ============================================================ */

static void signal_handler(int signal)
{
    (void)signal;
    running = 0;
}


/* ============================================================
 * Get current timestamp
 *
 * Format:
 *
 *     YYYY-MM-DD HH:MM:SS.mmm
 *
 * ============================================================ */

static void get_timestamp(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm local_time;

    now = time(NULL);

#ifdef _WIN32

    localtime_s(&local_time, &now);

#else

    localtime_r(&now, &local_time);

#endif

    /*
     * Get milliseconds.
     */
#ifdef _WIN32

    {
        SYSTEMTIME st;

        GetLocalTime(&st);

        snprintf(
            buffer,
            buffer_size,
            "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            st.wMilliseconds
        );
    }

#else

    {
        struct timeval tv;

        gettimeofday(&tv, NULL);

        snprintf(
            buffer,
            buffer_size,
            "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
            local_time.tm_year + 1900,
            local_time.tm_mon + 1,
            local_time.tm_mday,
            local_time.tm_hour,
            local_time.tm_min,
            local_time.tm_sec,
            tv.tv_usec / 1000
        );
    }

#endif
}


/* ============================================================
 * Create CSV filename
 * ============================================================ */

static void create_csv_filename(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm local_time;

    now = time(NULL);

#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif

    snprintf(
        buffer,
        buffer_size,
        "serial_log_%04d-%02d-%02d_%02d-%02d-%02d.csv",
        local_time.tm_year + 1900,
        local_time.tm_mon + 1,
        local_time.tm_mday,
        local_time.tm_hour,
        local_time.tm_min,
        local_time.tm_sec
    );
}


/* ============================================================
 * Serial initialization - Windows
 * ============================================================ */

#ifdef _WIN32

static int serial_open(const char *port)
{
    char device_name[64];

    /*
     * COM ports above COM9 need the special
     * Windows device path.
     */
    if (strncmp(port, "\\\\.\\", 4) == 0) {
        snprintf(
            device_name,
            sizeof(device_name),
            "%s",
            port
        );
    }
    else {
        snprintf(
            device_name,
            sizeof(device_name),
            "\\\\.\\%s",
            port
        );
    }

    serial_handle = CreateFileA(
        device_name,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (serial_handle == INVALID_HANDLE_VALUE) {
        fprintf(
            stderr,
            "ERROR: Could not open serial port '%s'\n",
            port
        );

        return 0;
    }

    /*
     * Configure serial port.
     */
    DCB dcb;

    memset(&dcb, 0, sizeof(dcb));

    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(serial_handle, &dcb)) {
        fprintf(stderr, "ERROR: GetCommState failed.\n");
        CloseHandle(serial_handle);
        serial_handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    dcb.BaudRate = 115000;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    /*
     * Disable hardware/software flow control.
     */
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(serial_handle, &dcb)) {
        fprintf(stderr, "ERROR: SetCommState failed.\n");
        CloseHandle(serial_handle);
        serial_handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    /*
     * Read timeout configuration.
     *
     * This allows the main loop to periodically check
     * whether the user requested termination.
     */
    COMMTIMEOUTS timeouts;

    memset(&timeouts, 0, sizeof(timeouts));

    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(serial_handle, &timeouts)) {
        fprintf(stderr, "ERROR: SetCommTimeouts failed.\n");
        CloseHandle(serial_handle);
        serial_handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    /*
     * Clear any old data sitting in the serial buffer.
     */
    PurgeComm(
        serial_handle,
        PURGE_RXCLEAR | PURGE_TXCLEAR
    );

    return 1;
}


/* ============================================================
 * Serial read - Windows
 * ============================================================ */

static int serial_read_byte(unsigned char *byte)
{
    DWORD bytes_read = 0;

    if (!ReadFile(
        serial_handle,
        byte,
        1,
        &bytes_read,
        NULL
    )) {
        return -1;
    }

    if (bytes_read == 0) {
        return 0;
    }

    return 1;
}


/* ============================================================
 * Serial close - Windows
 * ============================================================ */

static void serial_close(void)
{
    if (serial_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(serial_handle);
        serial_handle = INVALID_HANDLE_VALUE;
    }
}


#else   /* =====================================================
         * Linux / Unix
         * ===================================================== */


/* ============================================================
 * Serial initialization - Linux
 * ============================================================ */

static int serial_open(const char *port)
{
    serial_fd = open(
        port,
        O_RDWR | O_NOCTTY
    );

    if (serial_fd < 0) {
        fprintf(
            stderr,
            "ERROR: Could not open serial port '%s': %s\n",
            port,
            strerror(errno)
        );

        return 0;
    }

    struct termios tty;

    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_fd, &tty) != 0) {
        fprintf(
            stderr,
            "ERROR: tcgetattr failed: %s\n",
            strerror(errno)
        );

        close(serial_fd);
        serial_fd = -1;

        return 0;
    }

    /*
     * Raw serial mode.
     */
    cfmakeraw(&tty);

    /*
     * 115000 baud.
     *
     * B115200 is standard on most systems, but the requested
     * rate is 115000. Linux termios may not provide B115000.
     *
     * We therefore attempt B115200 if B115000 isn't available.
     *
     * See note below.
     */
#ifdef B115000
    cfsetispeed(&tty, B115000);
    cfsetospeed(&tty, B115000);
#else
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    fprintf(
        stderr,
        "WARNING: B115000 is unavailable on this system.\n"
        "         Using 115200 baud instead.\n"
    );
#endif

    /*
     * 8 data bits.
     */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    /*
     * No parity.
     */
    tty.c_cflag &= ~PARENB;

    /*
     * One stop bit.
     */
    tty.c_cflag &= ~CSTOPB;

    /*
     * Disable hardware flow control.
     */
    tty.c_cflag &= ~CRTSCTS;

    /*
     * Enable receiver.
     */
    tty.c_cflag |= CREAD;

    /*
     * Local connection.
     */
    tty.c_cflag |= CLOCAL;

    /*
     * Read behavior:
     *
     * VMIN = 0
     * VTIME = 1
     *
     * read() waits up to 100 ms.
     */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    /*
     * Apply settings.
     */
    if (tcsetattr(
        serial_fd,
        TCSANOW,
        &tty
    ) != 0) {

        fprintf(
            stderr,
            "ERROR: tcsetattr failed: %s\n",
            strerror(errno)
        );

        close(serial_fd);
        serial_fd = -1;

        return 0;
    }

    /*
     * Clear old data.
     */
    tcflush(
        serial_fd,
        TCIOFLUSH
    );

    return 1;
}


/* ============================================================
 * Serial read - Linux
 * ============================================================ */

static int serial_read_byte(unsigned char *byte)
{
    ssize_t result;

    result = read(
        serial_fd,
        byte,
        1
    );

    if (result < 0) {
        if (errno == EINTR) {
            return 0;
        }

        return -1;
    }

    if (result == 0) {
        return 0;
    }

    return 1;
}


/* ============================================================
 * Serial close - Linux
 * ============================================================ */

static void serial_close(void)
{
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
}

#endif


/* ============================================================
 * Parse a telemetry line
 *
 * Expected:
 *
 * T:1125.00 S:0 I:4.59 V:22.66 L:0.00
 *
 * We intentionally don't care about T.
 *
 * Returns:
 *     1 = valid telemetry line
 *     0 = not telemetry
 * ============================================================ */

static int parse_telemetry(
    const char *line,
    float *speed,
    float *current,
    float *voltage,
    float *load
)
{
    float ignored_time;

    /*
     * We use %f for each field and explicitly require
     * the expected labels.
     *
     * Whitespace before each field is accepted.
     */
    int matched = sscanf(
        line,
        " T:%f S:%f I:%f V:%f L:%f",
        &ignored_time,
        speed,
        current,
        voltage,
        load
    );

    /*
     * Exactly five values must be parsed.
     */
    if (matched == 5) {
        return 1;
    }

    return 0;
}


/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char *argv[])
{
    const char *port = NULL;

    /*
     * --------------------------------------------------------
     * Parse command line
     * --------------------------------------------------------
     */

    for (int i = 1; i < argc; i++) {

        if (
            strcmp(argv[i], "-port") == 0 ||
            strcmp(argv[i], "--port") == 0
        ) {

            if (i + 1 >= argc) {
                fprintf(
                    stderr,
                    "ERROR: -port requires a port name.\n"
                );

                return 1;
            }

            port = argv[++i];
        }
        else if (
            strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0
        ) {

            printf(
                "Usage:\n"
                "  %s -port PORT\n\n"
                "Examples:\n"
                "  Linux:   %s -port /dev/ttyUSB1\n"
                "  Windows: %s -port COM3\n\n"
                "Serial settings:\n"
                "  Baud: 115000\n"
                "  Data: 8\n"
                "  Parity: None\n"
                "  Stop: 1\n",
                argv[0],
                argv[0],
                argv[0]
            );

            return 0;
        }
        else {
            fprintf(
                stderr,
                "ERROR: Unknown argument '%s'\n",
                argv[i]
            );

            return 1;
        }
    }

    if (port == NULL) {
        fprintf(
            stderr,
            "ERROR: No serial port specified.\n\n"
            "Usage:\n"
            "  %s -port /dev/ttyUSB1\n"
            "  %s -port COM3\n",
            argv[0],
            argv[0]
        );

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Signal handling
     * --------------------------------------------------------
     */

    signal(SIGINT, signal_handler);

#ifdef SIGTERM
    signal(SIGTERM, signal_handler);
#endif


    /*
     * --------------------------------------------------------
     * Open serial port
     * --------------------------------------------------------
     */

    printf(
        "Opening serial port: %s\n",
        port
    );

    if (!serial_open(port)) {
        return 1;
    }

    printf(
        "Serial port opened successfully.\n"
    );

    printf(
        "Waiting for telemetry...\n"
    );


    /*
     * --------------------------------------------------------
     * Create CSV
     * --------------------------------------------------------
     */

    char csv_filename[256];

    create_csv_filename(
        csv_filename,
        sizeof(csv_filename)
    );

    FILE *csv = fopen(
        csv_filename,
        "w"
    );

    if (csv == NULL) {

        fprintf(
            stderr,
            "ERROR: Could not create CSV file '%s'.\n",
            csv_filename
        );

        serial_close();

        return 1;
    }

    /*
     * CSV header.
     */
    fprintf(
        csv,
        "Timestamp,Speed,Current,Voltage,Load\n"
    );

    fflush(csv);

    printf(
        "Logging to: %s\n",
        csv_filename
    );

    printf(
        "Press Ctrl+C to stop.\n\n"
    );


    /*
     * --------------------------------------------------------
     * Serial line buffer
     * --------------------------------------------------------
     */

    #define LINE_BUFFER_SIZE 1024

    char line[LINE_BUFFER_SIZE];
    size_t line_length = 0;


    /*
     * --------------------------------------------------------
     * Main read loop
     * --------------------------------------------------------
     */

    while (running) {

        unsigned char byte;

        int result = serial_read_byte(&byte);

        if (result < 0) {

            fprintf(
                stderr,
                "\nERROR: Serial read failed.\n"
            );

            break;
        }

        if (result == 0) {
            /*
             * No byte available right now.
             */
            continue;
        }


        /*
         * ----------------------------------------------------
         * Newline
         * ----------------------------------------------------
         */

        if (byte == '\n') {

            /*
             * Null terminate the line.
             */
            line[line_length] = '\0';

            /*
             * Remove trailing CR if this is Windows-style
             * CRLF data.
             */
            if (
                line_length > 0 &&
                line[line_length - 1] == '\r'
            ) {
                line[line_length - 1] = '\0';
            }


            /*
             * ------------------------------------------------
             * Try to parse telemetry.
             * ------------------------------------------------
             */

            float speed;
            float current;
            float voltage;
            float load;

            if (parse_telemetry(
                line,
                &speed,
                &current,
                &voltage,
                &load
            )) {

                /*
                 * Get computer's current time.
                 */
                char timestamp[64];

                get_timestamp(
                    timestamp,
                    sizeof(timestamp)
                );


                /*
                 * Write CSV row.
                 */
                fprintf(
                    csv,
                    "%s,%.3f,%.3f,%.3f,%.3f\n",
                    timestamp,
                    speed,
                    current,
                    voltage,
                    load
                );


                /*
                 * Flush immediately.
                 *
                 * This means if the program crashes or the
                 * computer loses power, we lose at most the
                 * current sample rather than a large buffer.
                 */
                fflush(csv);


                /*
                 * Also print the received data to the terminal.
                 */
                printf(
                    "%s | Speed: %.3f | Current: %.3f | "
                    "Voltage: %.3f | Load: %.3f\n",
                    timestamp,
                    speed,
                    current,
                    voltage,
                    load
                );
            }


            /*
             * Reset line buffer.
             */
            line_length = 0;

        }
        else {

            /*
             * ------------------------------------------------
             * Add byte to current line.
             * ------------------------------------------------
             */

            if (line_length < LINE_BUFFER_SIZE - 1) {

                line[line_length++] = (char)byte;

            }
            else {

                /*
                 * Line was too long.
                 *
                 * Discard it and wait for the next newline.
                 */
                line_length = 0;
            }
        }
    }


    /*
     * --------------------------------------------------------
     * Shutdown
     * --------------------------------------------------------
     */

    printf(
        "\nStopping logger...\n"
    );

    fflush(csv);

    fclose(csv);

    serial_close();

    printf(
        "CSV saved to: %s\n",
        csv_filename
    );

    return 0;
}
