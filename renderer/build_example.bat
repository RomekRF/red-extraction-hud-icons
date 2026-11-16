@echo off
rem Builds example_icon_render_lib.exe (the in-process icon_render_lib.h example) with Visual Studio 2022, like build.bat.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio 2022 not found. & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo No C++ tools in Visual Studio. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /EHsc /std:c++17 /W4 /O2 example_icon_render_lib.cpp src\v3m_model.cpp src\mesh_render.cpp /Foobj\ /Fe:example_icon_render_lib.exe
if errorlevel 1 exit /b 1
echo Built example_icon_render_lib.exe
