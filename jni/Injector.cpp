#include <getopt.h>
#include <cstring>
#include <unistd.h>

#include "Headers/RevMemory.h"

void print_help() {
    printf("Usage: Injector [options]\n");
    printf("Options:\n");
    printf("  -p, --pid <package>         Set the package name\n");
    printf("  -l, --library <library>     Set the library path\n");
    printf("  -a, --auto_launch           Enable auto launch\n");
    printf("  --launcher <launcher>       Set the launcher (required with --auto_launch)\n");
    printf("  -r, --remap                 Enable remap\n");
    printf("  -h, --help                  Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    bool auto_launch = false;
    bool remap = false;
    const char* native_pid = nullptr;
    const char* native_library = nullptr;
    const char* launcher_activity = nullptr;
    
    struct option long_options[] = {
        {"pid",          required_argument, 0, 'p'},
        {"library",      required_argument, 0, 'l'},
        {"auto_launch",  required_argument, 0, 'a'},
        {"remap",        required_argument, 0, 'r'},
        {"help",         required_argument, 0, 'h'},
        {0,              0,                 0,  0 }
    };

    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "p:l:aL:rh", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'p':
                native_pid = optarg;
                break;
            case 'l':
                native_library = optarg;
                break;
            case 'a':
                auto_launch = true;
                break;
            case 'L':
                launcher_activity = optarg;
                break;
            case 'r':
                remap = true;
                break;
            case 'h':
            default: 
                print_help();
                return 0;
        }
    }
    
    LOGI("[+] PID: %s", native_pid);
    LOGI("[+] Library: %s", native_library);
    LOGI("[+] Auto Launch: %s", auto_launch ? "Enabled" : "Disabled");
    LOGI("[+] Launcher: %s", launcher_activity);
    LOGI("[+] Remap: %s", remap ? "Enabled" : "Disabled");

    if (!native_pid || !native_library || (auto_launch && !launcher_activity)) {
        LOGE("[-] Missing required arguments");
        print_help();
        return 0;
    }

    // Launch Game if option is used
    if (auto_launch) {
        RevMemory::launch_app(launcher_activity);
    }

    pid_t pid = -1;

    // safer than atoi, can detect errors
    char *endptr;
    long val = strtol(native_pid, &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Invalid PID: %s\n", native_pid);
        return 1;
    }
    if (val <= 0) {
        fprintf(stderr, "PID must be positive\n");
        return 1;
    }

    pid = (pid_t)val;

    // Handle SELinux
    // NOTE: Causes crashes on emulators, most emulators don't need selinux
    bool is_emulator = false;
    #if defined(__arm__) || defined(__aarch64__)
        is_emulator = false;
        RevMemory::set_selinux(0);
    #elif defined(__x86_64__) || defined(__i386__)
        is_emulator = true;
    #endif

    // Check if we are on an emulator and then check the target library.
    // If the target library fits is also a x86/x86_64 library, we can
    // just load it normally, if not we have to do native bridge injection.
    if (is_emulator) {
        // Check Library Architecture
        ELFParser::MachineType type = ELFParser::getMachineType(native_library);

        // Needs to be loaded through native bridge
        if (type == ELFParser::MachineType::ELF_EM_AARCH64 || type == ELFParser::MachineType::ELF_EM_ARM) {
            LOGI("[+] Detected arm or aarch64 library on Emulator, starting emulator injection...");
            int result = RevMemory::EmulatorInject(pid, native_library, remap);
            LOGI("[+] Finished Emulator Injection with result %d", result);

            // Return the injection result
            return result;
        }
    }

    // Inject Normally
    int result = RevMemory::Inject(pid, native_library, remap);
    LOGI("[+] Finished Injection with result %d", result);

    // Restore SELinux
    #if defined(__arm__) || defined(__aarch64__)
        RevMemory::set_selinux(1);
    #endif

    return result;
}
