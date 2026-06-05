#include <stdint.h>

void _start(void) {
    const char *msg = "[ELF] Hello from a real compiled ELF user program running in Ring 3!\n";
    int len = 0;
    while (msg[len]) {
        len++;
    }

    // Call sys_write (syscall 0)
    asm volatile (
        "int $0x80"
        :
        : "a"(0),                       // EAX = 0 (SYSCALL_WRITE)
          "b"(1),                       // EBX = 1 (stdout)
          "c"((uint32_t)msg),           // ECX = buffer pointer
          "d"(len)                      // EDX = length
        : "memory"
    );

    // Call sys_exit (syscall 1) with status code 5
    asm volatile (
        "int $0x80"
        :
        : "a"(1),                       // EAX = 1 (SYSCALL_EXIT)
          "b"(5)                        // EBX = 5 (status code)
        : "memory"
    );

    // Loop forever if exit fails
    for (;;) {
        asm volatile("hlt");
    }
}
