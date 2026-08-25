set PATH=C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\MinGW\bin;%PATH%
set QT_PLUGIN_PATH=C:\Qt\6.11.1\mingw_64\plugins
cd /d E:\pos--2\build-ui
pos_core_tests.exe -o core_result.txt,txt
echo CORE_RC=%errorlevel%
