@ECHO OFF

REM Copyright Code Respawn Technologies Pvt. Ltd.

SET ENGINE_SOURCE_DIR=C:\UnrealEngine\Source\UnrealEngine
set OPENVDB_VERSION=9.1.0

setlocal

set PATH_TO_THIRDPARTY=%ENGINE_SOURCE_DIR%\Engine\Source\ThirdParty

if not exist %PATH_TO_THIRDPARTY% goto invalid_engine_path


set PATH_SCRIPT=%~dp0
set PATH_TO_CMAKE_FILE=%PATH_SCRIPT%

if not exist %PATH_TO_CMAKE_FILE% goto invalid_ovd_version

REM Temporary build directories (used as working directories when running CMake)
set OPENVDB_INTERMEDIATE_PATH=.\build

REM Install directories
set OPENVDB_INSTALL_PATH=%PATH_SCRIPT%\install
set OPENVDB_INSTALL_INCLUDE_PATH=%OPENVDB_INSTALL_PATH%\include
set OPENVDB_INSTALL_BIN_PATH=%OPENVDB_INSTALL_PATH%\bin\Win64\VS2019
set OPENVDB_INSTALL_LIB_PATH=%OPENVDB_INSTALL_PATH%\lib\Win64\VS2019

REM Dependency - ZLib
set PATH_TO_ZLIB=%PATH_TO_THIRDPARTY%\zlib\v1.2.8
set PATH_TO_ZLIB_SRC=%PATH_TO_ZLIB%\include\Win64\VS2015
set PATH_TO_ZLIB_LIB=%PATH_TO_ZLIB%\lib\Win64\VS2015\Release
set ZLIB_LIBRARY=%PATH_TO_ZLIB_LIB%\zlibstatic.lib

REM Dependency - Intel TBB
set PATH_TO_TBB=%PATH_TO_THIRDPARTY%\Intel\TBB\IntelTBB-2019u8
set PATH_TO_TBB_SRC=%PATH_TO_TBB%\include
set PATH_TO_TBB_LIB=%PATH_TO_TBB%\lib\Win64\vc14

REM Dependency - Blosc
set PATH_TO_BLOSC_SRC=%PATH_TO_THIRDPARTY%\Blosc\Deploy\c-blosc-1.5.0\include
set PATH_TO_BLOSC_LIB=%PATH_TO_THIRDPARTY%\Blosc\Deploy\c-blosc-1.5.0\lib\Win64\VS2019\Release
set Blosc_LIBRARY=%PATH_TO_BLOSC_LIB%\libblosc.lib

REM Dependency - Boost
set BOOST_ROOT=%PATH_TO_THIRDPARTY%\Boost\boost-1_70_0
set BOOST_INCL_DIR=%BOOST_ROOT%\include
set BOOST_LIBRARYDIR=%BOOST_ROOT%\lib\Win64

REM Clean destination folders
if exist %OPENVDB_INSTALL_PATH% (rmdir %OPENVDB_INSTALL_PATH% /s/q)

if exist %OPENVDB_INTERMEDIATE_PATH% (rmdir %OPENVDB_INTERMEDIATE_PATH% /s/q)
mkdir %OPENVDB_INTERMEDIATE_PATH%
pushd %OPENVDB_INTERMEDIATE_PATH%

REM Build OpenVDB Release for VS2019 (64-bit)
cmake -G "Visual Studio 16 2019" -A x64^
	-DCMAKE_PREFIX_PATH="%PATH_TO_ZLIB_SRC%;%PATH_TO_TBB_SRC%;%PATH_TO_TBB_LIB%;%PATH_TO_BLOSC_SRC%;%PATH_TO_BLOSC_LIB%"^
	-DCMAKE_INSTALL_PREFIX="%OPENVDB_INSTALL_PATH%"^
	-DCMAKE_INSTALL_INCLUDEDIR="%OPENVDB_INSTALL_INCLUDE_PATH%"^
	-DCMAKE_INSTALL_BINDIR="%OPENVDB_INSTALL_BIN_PATH%"^
	-DCMAKE_INSTALL_LIBDIR="%OPENVDB_INSTALL_LIB_PATH%"^
	-DZLIB_LIBRARY="%ZLIB_LIBRARY%"^
	-DBlosc_LIBRARY="%Blosc_LIBRARY%"^
	-DBlosc_LIBRARY_RELEASE="%Blosc_LIBRARY%"^
	-DUSE_PKGCONFIG=OFF^
	-DOPENVDB_BUILD_CORE=ON -DOPENVDB_BUILD_BINARIES=OFF -DOPENVDB_INSTALL_CMAKE_MODULES=OFF^
	-DOPENVDB_CORE_SHARED=ON -DOPENVDB_CORE_STATIC=OFF^
	-DUSE_STATIC_DEPENDENCIES=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL^
	-DMSVC_MP_THREAD_COUNT="8"^
	%PATH_TO_CMAKE_FILE%
rem cmake --build . --target install --config Release -j8

popd

goto :eof

:usage
echo Usage: BuildForWindows <version>
exit /B 1


:invalid_engine_path
echo Invalid Engine Soruce Path: Please open the script and set the engine's source directory
exit /B 2

:invalid_ovd_version
echo Invalid OpenVDB Version: Please make sure the version number is correct (this number is in the library folder two parents above this directory: openvbd-x.x.x)
exit /B 3


endlocal

