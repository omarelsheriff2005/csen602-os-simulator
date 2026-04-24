@echo off
cd /d "D:\Operating System Project\os_sim"
D:\gcc\ucrt64\bin\gcc.exe -I. -c test_build.c -o test_build.o > "D:\Operating System Project\os_sim\build_log.txt" 2>&1
echo EXIT: %ERRORLEVEL% >> "D:\Operating System Project\os_sim\build_log.txt"
