$env:PATH = "D:\Qt\6.8.2\msvc2022_64\bin;" + $env:PATH.Replace('nodejs"', 'nodejs').Replace('"', '')
& cmd.exe /c "call `"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat`" -arch=x64 && cd /d `"d:\Udoo Key Project\udoo-key-app\build`" && ninja && ctest --output-on-failure -V"
