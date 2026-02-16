@echo off
title 驱动加载管理工具

REM 检查是否以管理员身份运行
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if '%errorlevel%' NEQ '0' (
    echo 请求管理员权限...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo 正在加载驱动...

REM 设置变量
set DRIVER_NAME=BobHWin7Driver
set DRIVER_PATH=%~dp0%BobHWin7Driver.sys
set EXE_PATH=%~dp0main.exe

REM 检查文件是否存在
if not exist "%DRIVER_PATH%" (
    echo 错误：找不到驱动文件 %DRIVER_PATH%
    pause
    exit /b 1
)

if not exist "%EXE_PATH%" (
    echo 错误：找不到可执行文件 %EXE_PATH%
    pause
    exit /b 1
)

REM 创建服务（使用SC命令）
echo 正在创建驱动服务...
sc create %DRIVER_NAME% binPath= "%DRIVER_PATH%" type= kernel start= demand >nul 2>&1

if errorlevel 1 (
    echo 服务创建失败，可能已存在或权限不足
    goto :cleanup
)

echo 服务创建成功

REM 启动驱动服务
echo 正在启动驱动服务...
sc start %DRIVER_NAME% >nul 2>&1

if errorlevel 1 (
    echo 驱动启动失败
    goto :cleanup
)

echo 驱动启动成功
echo.

REM 执行主程序
echo 正在启动 main.exe...
start /wait "" "%EXE_PATH%"
echo main.exe 已结束
echo.

:cleanup
REM 停止并删除服务
echo 正在清理驱动服务...

sc stop %DRIVER_NAME% >nul 2>&1
timeout /t 2 /nobreak >nul
sc delete %DRIVER_NAME% >nul 2>&1

if errorlevel 1 (
    echo 服务删除失败，可能需要手动清理
) else (
    echo 驱动服务已成功卸载
)

echo.
echo 操作完成
pause