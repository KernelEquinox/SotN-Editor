@echo off
for /F "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do set VCINSTALLDIR=%%i
call "%VCINSTALLDIR%\Common7\Tools\VsDevCmd.bat"
if not exist vcpkg\ (
    git clone https://github.com/microsoft/vcpkg.git
)
cmake -B build -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build
pause
