#include <Python.h>
#include <windows.h>
#include <winioctl.h>

// 与驱动程序通讯的缓冲区结构体
typedef struct {
    ULONG64 Address;
    ULONG64 Buffer;
    ULONG64 size;
} r3Buffer;

// IOCTL 控制码
#define BOBH_SET CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_PROTECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_UNPROTECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_KILLPROCESS_DIRECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BOBH_KILLPROCESS_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x816, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DEVICE_NAME L"\\\\.\\BobHWin7ReadLink"

// 全局句柄
static HANDLE g_hDevice = INVALID_HANDLE_VALUE;

// 打开驱动连接
static PyObject* bobh_open(PyObject* self, PyObject* args) {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
    }
    
    g_hDevice = CreateFileW(
        DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "无法打开驱动程序设备");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 关闭驱动连接
static PyObject* bobh_close(PyObject* self, PyObject* args) {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
        g_hDevice = INVALID_HANDLE_VALUE;
    }
    Py_RETURN_NONE;
}

// 设置目标PID
static PyObject* bobh_setpid(PyObject* self, PyObject* args) {
    DWORD pid;
    
    if (!PyArg_ParseTuple(args, "I", &pid)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_SET,
        &pid,
        sizeof(pid),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "设置PID失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 读取内存
static PyObject* bobh_read(PyObject* self, PyObject* args) {
    unsigned long long addr;
    unsigned long long size;
    
    if (!PyArg_ParseTuple(args, "KK", &addr, &size)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    // 分配缓冲区
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        PyErr_SetString(PyExc_MemoryError, "内存分配失败");
        return NULL;
    }
    
    // 准备IO缓冲区
    r3Buffer ioBuffer;
    ioBuffer.Address = addr;
    ioBuffer.Buffer = (ULONG64)buffer;
    ioBuffer.size = size;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_READ,
        &ioBuffer,
        sizeof(ioBuffer),
        &ioBuffer,
        sizeof(ioBuffer),
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        free(buffer);
        PyErr_SetString(PyExc_OSError, "读取内存失败");
        return NULL;
    }
    
    // 创建Python字节对象
    PyObject* py_bytes = PyBytes_FromStringAndSize((const char*)buffer, size);
    free(buffer);
    
    return py_bytes;
}

// 写入内存
static PyObject* bobh_write(PyObject* self, PyObject* args) {
    unsigned long long addr;
    PyObject* data_obj;
    
    if (!PyArg_ParseTuple(args, "KO", &addr, &data_obj)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    // 获取数据指针和长度
    Py_buffer view;
    if (PyObject_GetBuffer(data_obj, &view, PyBUF_SIMPLE) != 0) {
        return NULL;
    }
    
    unsigned long long size = view.len;
    unsigned char* data = (unsigned char*)view.buf;
    
    // 准备输入缓冲区
    size_t total_size = sizeof(r3Buffer) + size;
    unsigned char* input_buffer = (unsigned char*)malloc(total_size);
    if (!input_buffer) {
        PyBuffer_Release(&view);
        PyErr_SetString(PyExc_MemoryError, "内存分配失败");
        return NULL;
    }
    
    // 填充结构体
    r3Buffer* pIoHeader = (r3Buffer*)input_buffer;
    pIoHeader->Address = addr;
    pIoHeader->size = size;
    pIoHeader->Buffer = (ULONG64)(input_buffer + sizeof(r3Buffer));
    
    // 复制数据
    memcpy(input_buffer + sizeof(r3Buffer), data, size);
    
    // 发送IOCTL
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_WRITE,
        input_buffer,
        (DWORD)total_size,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    free(input_buffer);
    PyBuffer_Release(&view);
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "写入内存失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 保护进程
static PyObject* bobh_protect(PyObject* self, PyObject* args) {
    DWORD pid;
    
    if (!PyArg_ParseTuple(args, "I", &pid)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_PROTECT,
        &pid,
        sizeof(pid),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "进程保护失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 取消保护
static PyObject* bobh_unprotect(PyObject* self, PyObject* args) {
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_UNPROTECT,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "取消保护失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 直接终止进程
static PyObject* bobh_kill_direct(PyObject* self, PyObject* args) {
    DWORD pid;
    
    if (!PyArg_ParseTuple(args, "I", &pid)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_KILLPROCESS_DIRECT,
        &pid,
        sizeof(pid),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "终止进程失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 内存抹除后终止进程
static PyObject* bobh_kill_memory(PyObject* self, PyObject* args) {
    DWORD pid;
    
    if (!PyArg_ParseTuple(args, "I", &pid)) {
        return NULL;
    }
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        PyErr_SetString(PyExc_OSError, "驱动未连接");
        return NULL;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        g_hDevice,
        BOBH_KILLPROCESS_MEMORY,
        &pid,
        sizeof(pid),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        PyErr_SetString(PyExc_OSError, "内存抹除终止失败");
        return NULL;
    }
    
    Py_RETURN_TRUE;
}

// 模块方法定义
static PyMethodDef BobhMethods[] = {
    {"open", bobh_open, METH_VARARGS, "连接到驱动程序"},
    {"close", bobh_close, METH_VARARGS, "关闭驱动连接"},
    {"setpid", bobh_setpid, METH_VARARGS, "设置目标进程PID"},
    {"read", bobh_read, METH_VARARGS, "读取进程内存"},
    {"write", bobh_write, METH_VARARGS, "写入进程内存"},
    {"protect", bobh_protect, METH_VARARGS, "保护进程"},
    {"unprotect", bobh_unprotect, METH_VARARGS, "取消进程保护"},
    {"kill_direct", bobh_kill_direct, METH_VARARGS, "直接终止进程"},
    {"kill_memory", bobh_kill_memory, METH_VARARGS, "内存抹除后终止进程"},
    {NULL, NULL, 0, NULL}
};

// 模块定义
static struct PyModuleDef bobhmodule = {
    PyModuleDef_HEAD_INIT,
    "bobh_driver",   // 模块名
    "BobH驱动程序Python接口",  // 模块文档
    -1,
    BobhMethods
};

// 模块初始化函数
PyMODINIT_FUNC PyInit_bobh_driver(void) {
    return PyModule_Create(&bobhmodule);
}