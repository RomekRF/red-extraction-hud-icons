@echo off
rem Builds icon_render.exe with Visual Studio 2022 (Community or Build Tools, "Desktop development with C++").
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio 2022 not found. & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo No C++ tools in Visual Studio. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /EHsc /std:c++20 /O2 src\icon_render.cpp src\v3m_model.cpp src\mesh_render.cpp /Foobj\ /Fe:icon_render.exe
if errorlevel 1 exit /b 1
echo Built icon_render.exe
