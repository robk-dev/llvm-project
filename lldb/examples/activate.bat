@echo off
SET "CONAN_OLD_PATH=%PATH%"
SET "CONAN_OLD_PKG_CONFIG_PATH=%PKG_CONFIG_PATH%"

FOR /F "usebackq tokens=1,* delims==" %%i IN ("C:\code\llvm-project\lldb\examples\environment.bat.env") DO (
    CALL SET "%%i=%%j"
)

SET "CONAN_OLD_PROMPT=%PROMPT%"
SET "PROMPT=(conanenv) %PROMPT%"