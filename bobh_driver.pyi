"""
BobH驱动程序Python接口
Windows驱动程序通信模块，提供进程内存读写、进程保护和管理功能。
"""

import sys
from typing import Any, BinaryIO, Optional, Union

if sys.version_info >= (3, 8):
    from typing import Literal
else:
    from typing_extensions import Literal

def open() -> bool:
    """
    连接到驱动程序设备
    
    Returns:
        bool: 连接成功返回True，失败抛出OSError异常
        
    Raises:
        OSError: 如果无法打开驱动程序设备
    """
    ...

def close() -> None:
    """
    关闭驱动程序连接
    
    释放驱动句柄资源，应在程序结束时调用
    """
    ...

def setpid(pid: int) -> bool:
    """
    设置目标进程PID，供后续读写操作使用
    
    Args:
        pid: 目标进程ID
        
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或操作失败
    """
    ...

def read(address: int, size: int) -> bytes:
    """
    从目标进程读取指定地址的内存数据
    
    Args:
        address: 内存地址（十六进制整数）
        size: 要读取的字节数
        
    Returns:
        bytes: 读取到的内存数据
        
    Raises:
        OSError: 如果驱动未连接或读取失败
        MemoryError: 如果内存分配失败
    """
    ...

def write(address: int, data: Union[bytes, bytearray, memoryview]) -> bool:
    """
    向目标进程写入内存数据
    
    Args:
        address: 内存地址（十六进制整数）
        data: 要写入的数据（bytes、bytearray或memoryview对象）
        
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或写入失败
        MemoryError: 如果内存分配失败
        TypeError: 如果data参数类型不正确
    """
    ...

def protect(pid: int) -> bool:
    """
    保护指定进程，禁止其他程序以特定权限打开其句柄
    
    保护的操作包括：TERMINATE、VM_OPERATION、VM_READ、VM_WRITE
    
    Args:
        pid: 要保护的进程ID
        
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或操作失败
    """
    ...

def unprotect() -> bool:
    """
    停止进程保护
    
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或操作失败
    """
    ...

def kill_direct(pid: int) -> bool:
    """
    直接终止指定进程
    
    Args:
        pid: 要终止的进程ID
        
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或操作失败
    """
    ...

def kill_memory(pid: int) -> bool:
    """
    尝试抹除进程的用户态内存空间后终止进程
    
    先尝试清空进程内存，然后再终止进程
    
    Args:
        pid: 要终止的进程ID
        
    Returns:
        bool: 操作成功返回True
        
    Raises:
        OSError: 如果驱动未连接或操作失败
    """
    ...

# 模块级异常定义
class BobhError(Exception):
    """BobH驱动模块基类异常"""
    pass

class ConnectionError(BobhError):
    """驱动连接异常"""
    pass

class MemoryError(BobhError):
    """内存操作异常"""
    pass

class ProcessError(BobhError):
    """进程操作异常"""
    pass