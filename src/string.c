#include "string.h"

void *memset(void *dest, int val, size_t len) {
    unsigned char *temp = (unsigned char *)dest;
    for (size_t i = 0; i < len; i++) {
        temp[i] = (unsigned char)val;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
    const unsigned char *sp = (const unsigned char *)src;
    unsigned char *dp = (unsigned char *)dest;
    for (size_t i = 0; i < len; i++) {
        dp[i] = sp[i];
    }
    return dest;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *temp = dest;
    while ((*temp++ = *src++) != '\0') {
        /* copy characters */
    }
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
