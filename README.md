# BobHWin7DriverNew

BobHWin7DriverNew 是 [BobHWin7Driver](https://github.com/BobH233/BobHWinDriver) 的重构版，使这个驱动在最新环境下可编译且提供C语言的接入示例与Python接入API。

## 项目特点

一个简单的 Windows 内核驱动程序，包含内存读写、进程终止和 I/O 通信功能

## 项目结构

```
BobHWinDriverNew/
├── BobHWinDriver/          # 驱动源代码目录
│   ├── ...                 # 驱动源代码
├── main.c                  # C语言接入示例原代码
├── run.bat                 # C语言接入示例启动脚本（自动挂载驱动）
├── PythonAPI.c             # Python接入API的源代码
├── bobh_driver.pyi         # Python接入API的型存根文件
├── build.bat               # 编译脚本，视环境而定，仅供参考
├── LICENSE                 # 项目许可证文件
└── README                  # 项目说明文档
```

## 许可证

本项目采用Apache License Version 2.0开源协议

## 贡献

欢迎对本项目进行贡献，包括功能改进、Bug修复和文档完善。
