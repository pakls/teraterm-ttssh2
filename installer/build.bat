
rem Visual Studioのバージョン判別
echo %VSINSTALLDIR% | find "Studio 8"
if not ERRORLEVEL 1 goto vs2005

set TERATERMSLN=..\teraterm\ttermpro.v12.sln
set TTSSHSLN=..\ttssh2\ttssh.v12.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v12.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v12.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v12.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v12.sln
goto vsend

:vs2005
set TERATERMSLN=..\teraterm\ttermpro.sln
set TTSSHSLN=..\ttssh2\ttssh.sln
set TTPROXYSLN=..\TTProxy\TTProxy.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.sln

:vsend

set BUILD=build
if "%1" == "rebuild" (set BUILD=rebuild)

rem ライブラリをコンパイル
pushd ..\libs
CALL buildall.bat
popd

if "%BUILD%" == "rebuild" goto build

rem "rebuild"を指定しない場合、SVNリビジョンを更新する。
if exist ..\teraterm\release\svnrev.exe goto svnrev
devenv /build release %TERATERMSLN% /project svnrev /projectconfig release

:svnrev
..\teraterm\release\svnrev.exe ..\libs\svn\bin\svnversion.exe .. ..\teraterm\ttpdlg\svnversion.h

:build
devenv /%BUILD% release %TERATERMSLN%
devenv /%BUILD% release %TTSSHSLN%
devenv /%BUILD% release %TTPROXYSLN%
devenv /%BUILD% release %TTXKANJISLN%
devenv /%BUILD% release %TTPMENUSLN%
devenv /%BUILD% release %TTXSAMPLESLN%

rem cygterm をコンパイル
pushd ..\cygterm
if "%BUILD%" == "rebuild" make clean
make
popd

rem cygtool をコンパイル
pushd cygtool
nmake -f cygtool.mak
popd
