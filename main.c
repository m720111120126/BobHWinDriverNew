#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BOBH_SET CTL_CODE(FILE_DEVICE_UNKNOWN,0x810,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_READ CTL_CODE(FILE_DEVICE_UNKNOWN,0x811,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN,0x812,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_PROTECT CTL_CODE(FILE_DEVICE_UNKNOWN,0x813,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_UNPROTECT CTL_CODE(FILE_DEVICE_UNKNOWN,0x814,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_KILLPROCESS_DIRECT CTL_CODE(FILE_DEVICE_UNKNOWN,0x815,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define BOBH_KILLPROCESS_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN,0x816,METHOD_BUFFERED,FILE_ANY_ACCESS)

// 与驱动程序通讯的缓冲区结构体
struct r3Buffer {
    ULONG64 Address;
    ULONG64 Buffer;
    ULONG64 size;
};

// 驱动程序的符号链接名称
#define DEVICE_NAME L"\\\\.\\BobHWin7ReadLink"

void printUsage() {
    printf("驱动程序控制面板 使用说明:\n");
    printf("============================\n");
    printf("setpid <PID>              : 设置目标进程PID，供后续Read/Write使用。\n");
    printf("read <addr> <size>        : 从已设置的进程中读取指定地址和长度的内存。\n");
    printf("write <addr> <size> <hex> : 向已设置的进程中写入指定地址和长度的内存（内容为连续的十六进制字节）。\n");
    printf("protect <PID>             : 保护指定PID的进程，禁止其他程序以TERMINATE/VM_OPERATION/VM_READ/VM_WRITE权限打开其句柄。\n");
    printf("unprotect                 : 停止进程保护。\n");
    printf("kill_direct <PID>         : 直接终止指定PID的进程。\n");
    printf("kill_memory <PID>         : 尝试抹除指定PID进程的用户态内存空间后再终止它。\n");
    printf("exit                      : 退出控制面板。\n\n");
    printf("示例:\n");
    printf("  setpid 1234\n");
    printf("  read 0x400000 100\n");
    printf("  write 0x400000 4 AABBCCDD\n");
    printf("  protect 5678\n");
}

HANDLE openDriverHandle() {
    HANDLE hDevice = CreateFileW(
        DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[!] 无法打开驱动程序设备。错误代码: %d\n", GetLastError());
        return NULL;
    }
    printf("[+] 成功连接到驱动程序。\n");
    return hDevice;
}

BOOL callDriverControl(HANDLE hDevice, DWORD ioctlCode, LPVOID inBuffer, DWORD inSize, LPVOID outBuffer, DWORD outSize) {
    DWORD bytesReturned = 0;
    return DeviceIoControl(hDevice, ioctlCode, inBuffer, inSize, outBuffer, outSize, &bytesReturned, NULL);
}

// 辅助函数：将十六进制字符串（如"AABB"）转换为字节数组
int hexStringToBytes(const char* hexStr, BYTE* bytes, int maxBytes) {
    int len = strlen(hexStr);
    if (len % 2 != 0) return -1; // 必须是偶数长度
    int byteLen = len / 2;
    if (byteLen > maxBytes) return -1;

    for (int i = 0; i < byteLen; i++) {
        sscanf(hexStr + 2 * i, "%2hhx", &bytes[i]);
    }
    return byteLen;
}

int main() {
    HANDLE hDevice = openDriverHandle();
    if (hDevice == NULL) {
        printf("按回车键关闭...");
        getchar();
        return -1;
    }

    char command[256];
    char* token;
    DWORD pid;
    ULONG64 addr, size;
    struct r3Buffer ioBuffer;

    printUsage();

    while (1) {
        printf("\ncmd> ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;

        // 移除末尾的换行符
        command[strcspn(command, "\n")] = 0;
        if (strlen(command) == 0) continue;

        token = strtok(command, " ");

        if (_stricmp(token, "exit") == 0) {
            printf("[*] 正在退出...\n");
            break;
        }
        else if (_stricmp(token, "setpid") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: setpid <PID>\n");
                continue;
            }
            pid = atoi(token);
            if (callDriverControl(hDevice, BOBH_SET, &pid, sizeof(pid), NULL, 0)) {
                printf("[+] SETPID 命令发送成功。\n");
            }
            else {
                printf("[!] SETPID 命令失败。错误: %d\n", GetLastError());
            }
        }
        else if (_stricmp(token, "read") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: read <十六进制地址> <大小>\n");
                continue;
            }
            sscanf(token, "%llx", &addr);
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: read <十六进制地址> <大小>\n");
                continue;
            }
            sscanf(token, "%llu", &size);

            // 分配缓冲区存放读取结果
            BYTE* readBuffer = (BYTE*)malloc(size);
            if (!readBuffer) {
                printf("[!] 内存分配失败。\n");
                continue;
            }

           // 填充IO缓冲区结构
           ioBuffer.Address = addr;
           ioBuffer.Buffer = (ULONG64)readBuffer; // 请注意：这里传递的是用户态缓冲区地址，驱动会将数据复制到这里。
           ioBuffer.size = size;
           // 注意：驱动程序中的 DispatchDevCTL 函数，在处理 BOBH_READ 时，
           // 会从 appBuffer.Buffer（一个用户态地址）读取输出缓冲区的位置。
           // 在 METHOD_BUFFERED 方式下，SystemBuffer 是内核与用户态之间的拷贝桥梁。
           // 我们的结构体 ioBuffer 本身就是输入/输出缓冲区。
           // 根据驱动程序逻辑，我们需要将整个 ioBuffer 结构作为输入和输出缓冲区。
            if (callDriverControl(hDevice, BOBH_READ, &ioBuffer, sizeof(ioBuffer), &ioBuffer, sizeof(ioBuffer))) {
                printf("[+] 读取成功。开始显示内容（最多256字节）:\n");
                // 注意：读取的数据现在在我们分配的 readBuffer 中。
                DWORD displaySize = size > 256 ? 256 : size;
                for (DWORD i = 0; i < displaySize; i++) {
                    printf("%02X ", readBuffer[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                }
                printf("\n");
            }
            else {
                printf("[!] 读取失败。错误: %d\n", GetLastError());
            }
            free(readBuffer);
        }
        else if (_stricmp(token, "write") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: write <十六进制地址> <大小> <十六进制数据>\n");
                continue;
            }
            sscanf(token, "%llx", &addr);
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: write <十六进制地址> <大小> <十六进制数据>\n");
                continue;
            }
            sscanf(token, "%llu", &size);
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: write <十六进制地址> <大小> <十六进制数据>\n");
                continue;
            }

            // 将十六进制字符串转为字节
            BYTE* writeData = (BYTE*)malloc(size);
            if (!writeData) {
                printf("[!] 内存分配失败。\n");
                continue;
            }
            int dataLen = hexStringToBytes(token, writeData, size);
            if (dataLen != size) {
                printf("[!] 提供的数据长度与指定大小不符，或解析错误。\n");
                free(writeData);
                continue;
            }

            // 填充IO缓冲区结构
            // 注意：驱动程序的写入操作期望 appBuffer.Buffer 指向一个包含要写入数据的缓冲区。
            // 我们需要将数据复制到一个连续的内存块，并将其地址赋给 ioBuffer.Buffer。
            // 为了简单起见，我们可以将要写入的数据直接放在 ioBuffer 结构后面，但这里我们使用独立的缓冲区。
            // 我们将使用 SystemBuffer 传递两部分数据：ioBuffer结构头 + 实际数据。
            // 但根据驱动程序代码，它期望输入缓冲区是一个 r3Buffer 结构体，其中 Buffer 成员是一个用户态地址（它会从中拷贝数据）。
            // 这在 METHOD_BUFFERED 下是安全的，因为 SystemBuffer 是拷贝。

            // 因此，我们需要构建一个包含结构头和数据的输入缓冲区。
            DWORD totalInputSize = sizeof(struct r3Buffer) + size;
            BYTE* inputBuffer = (BYTE*)malloc(totalInputSize);
            if (!inputBuffer) {
                printf("[!] 输入缓冲区分配失败。\n");
                free(writeData);
                continue;
            }

            // 前部分是结构体
            struct r3Buffer* pIoHeader = (struct r3Buffer*)inputBuffer;
            pIoHeader->Address = addr;
            pIoHeader->size = size;
            // Buffer 成员指向紧随结构体之后的数据区（在 SystemBuffer 内部）
            pIoHeader->Buffer = (ULONG64)(inputBuffer + sizeof(struct r3Buffer));

            // 将要写入的数据拷贝到输入缓冲区的数据区
            memcpy(inputBuffer + sizeof(struct r3Buffer), writeData, size);

            if (callDriverControl(hDevice, BOBH_WRITE, inputBuffer, totalInputSize, NULL, 0)) {
                printf("[+] 写入成功。\n");
            }
            else {
                printf("[!] 写入失败。错误: %d\n", GetLastError());
            }

            free(writeData);
            free(inputBuffer);
        }
        else if (_stricmp(token, "protect") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: protect <PID>\n");
                continue;
            }
            pid = atoi(token);
            if (callDriverControl(hDevice, BOBH_PROTECT, &pid, sizeof(pid), NULL, 0)) {
                printf("[+] 进程保护已启动。\n");
            }
            else {
                printf("[!] 进程保护启动失败。错误: %d\n", GetLastError());
            }
        }
        else if (_stricmp(token, "unprotect") == 0) {
            if (callDriverControl(hDevice, BOBH_UNPROTECT, NULL, 0, NULL, 0)) {
                printf("[+] 进程保护已停止。\n");
            }
            else {
                printf("[!] 停止进程保护失败。错误: %d\n", GetLastError());
            }
        }
        else if (_stricmp(token, "kill_direct") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: kill_direct <PID>\n");
                continue;
            }
            pid = atoi(token);
            if (callDriverControl(hDevice, BOBH_KILLPROCESS_DIRECT, &pid, sizeof(pid), NULL, 0)) {
                printf("[+] 直接终止命令已发送。\n");
            }
            else {
                printf("[!] 终止命令失败。错误: %d\n", GetLastError());
            }
        }
        else if (_stricmp(token, "kill_memory") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("[!] 使用方法: kill_memory <PID>\n");
                continue;
            }
            pid = atoi(token);
            if (callDriverControl(hDevice, BOBH_KILLPROCESS_MEMORY, &pid, sizeof(pid), NULL, 0)) {
                printf("[+] 内存抹除终止命令已发送。\n");
            }
            else {
                printf("[!] 内存抹除终止命令失败。错误: %d\n", GetLastError());
            }
        }
        else if (_stricmp(token, "help") == 0) {
            printUsage();
        }
        else {
            printf("[!] 未知命令 '%s'。输入 'help' 查看使用说明。\n", token);
        }
    }

    CloseHandle(hDevice);
    return 0;
}