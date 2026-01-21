#include "xprintf.h"
#include "usart.h"
void my_putchar(char c) {
    // putchar(c);
    HAL_UART_Transmit( &huart1, (uint8_t*)&c, 1, HAL_MAX_DELAY);
}

void my_puts(const char *str) {
    while (*str) {
        my_putchar(*str);
        str++;
    }
}

void my_putint(int num) {
    if (num == 0) {
        my_putchar('0');
        return;
    }

    if (num < 0) {
        my_putchar('-');
        num = -num;
    }

    char buffer[20];
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) {
        my_putchar(buffer[--i]);
    }
}

void my_puthex(int num) {
    if (num == 0) {
        my_putchar('0');
        return;
    }

    char buffer[20];
    int i = 0;
    unsigned char ch;
    while (num > 0) {
        ch = num & 0xf;
        if(ch < 10)
        {
            buffer[i++] = '0' + (ch % 10);
        }
        else
        {
            buffer[i++] = 'A' + (ch - 10);
        }
        num >>= 4;
    }

    while (i > 0) {
        my_putchar(buffer[--i]);
    }
}

void my_putfloat(double num) {
    // 实现浮点数的输出需要更复杂的逻辑
    // 这里简化为输出小数点后两位
    int integerPart = (int)num;
    double fractionalPart = num - integerPart;

    my_putint(integerPart);
    my_putchar('.');
    my_putint((int)(fractionalPart * 100));
}

void xprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++; // Move past '%'
            switch (*format) {
                case 'd':
                    my_putint(va_arg(args, int));
                    break;
                case 's':
                    my_puts(va_arg(args, char *));
                    break;
                case 'f':
                    my_putfloat(va_arg(args, double));
                    break;
                case 'x':
                    my_puthex(va_arg(args, unsigned int));
                    break;
                case 'c':
                    my_putchar(va_arg(args, int));
                    break;
                default:
                    my_putchar('%');
                    my_putchar(*format);
                    break;
            }
        } else {
            my_putchar(*format);
        }
        format++;
    }

    va_end(args);
}
