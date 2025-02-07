#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <cmath>
#include <climits>
#include <sys/ucontext.h>


static const int MEMORY_DUMP_RANGE = 20;
const char* registers[23] = {"R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
    "RDI", "RSI", "RBP", "RBX", "RDX", "RAX", "RCX", "RSP", "RIP", "EFL", "CSGSFS",
    "ERR", "TRAPNO", "OLDMASK", "CR2"};

void writeNumHex(const uint8_t num) {
    char c = num + (num < 10 ? '0' : 'A' - 10);
    write(1, &c, 1);
}

void writeByteHex(const uint8_t byte) {
    writeNumHex(byte >> (unsigned) 4);
    writeNumHex(byte & (unsigned) 0xF);
}

void writeSafe(const uint64_t val) {
    for (int i = 7; i >= 0; --i) {
        writeByte(0xFF & (val >> (8 * i)));
    }
}

void writeSafe(const char* string) {
    write(1, string, strlen(string));
}

void writeError(const char* message) {
    write(2, message, strlen(message));
}

void dumpMemory(char* address) {
    writeSafe("\nMemory dump:\n");
    int p[2];
    if (pipe(p) == -1) {
        writeError("Error while creating pipe");
        writeError(strerror(errno));
        _exit(EXIT_FAILURE);
    }
    for (long long i = -MEMORY_DUMP_RANGE; i <= MEMORY_DUMP_RANGE; i++) {
        char* addr = address + i;
        writeSafe("0x");
        writeSafe((uint64_t) addr);
        writeSafe(": ");
        uint8_t val;
        if (write(p[1], addr, 1) == -1 || read(p[0], &val, 1) == -1) {
            writeSafe("Couldn't dump");
        } else {
            writeByte(val);
        }
        writeSafe("\n");
    }
}

void dumpRegisters(ucontext_t* ucontext) {
    writeSafe("\nGeneral purposes registers:\n");
    greg_t* gregs = ucontext->uc_mcontext.gregs;
    for (greg_t curReg = REG_R8; curReg != NGREG; curReg++) {
        writeSafe(registers[curReg]);
        writeSafe(": 0x");
        writeSafe(gregs[curReg]);
        writeSafe("\n");
    }
}

void sigsegvHandler(int signo, siginfo_t* siginfo, void* context) {
    if (siginfo->si_signo == SIGSEGV) {
        writeSafe("Signal aborted: ");
        writeSafe((uint64_t)siginfo->si_signo);
        writeSafe("\n");
        switch (siginfo->si_code) {
            case SEGV_MAPERR: {
                writeSafe("\nCause: Address is not mapped to object\n");
                break;
            }
            case SEGV_ACCERR: {
                writeSafe("\nCause: Invalid permission for mapped object\n");
                break;
            }
            default:
                writeSafe("\nUnsupported code\n");
        }
        dumpRegisters(reinterpret_cast<ucontext_t*> (context));
        dumpMemory((char*)siginfo->si_addr);
    }
    _exit(EXIT_FAILURE);
}

void test1() {
    char* arr = const_cast<char*>("123");
    arr[21] = '4';
}

void test2() {
    int* x = nullptr;
    *x = 21;
}

int main(int argc, char **argv) {
    struct sigaction action{};
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = sigsegvHandler;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &action, nullptr) < 0) {
        writeError("Sigaction failed\n");
        writeError(strerror(errno));
        _exit(EXIT_FAILURE);
    }
//    test1();
    test2();
    _exit(EXIT_SUCCESS);
}
