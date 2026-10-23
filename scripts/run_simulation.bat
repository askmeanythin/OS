@echo off
cd /d %~dp0..
if not exist config\server.conf copy config\server.example.conf config\server.conf
echo Simulation mode: one computer, 8 simulated PC agents over loopback TCP.
echo This demonstrates the workflow; it is NOT proof of multi-computer testing.
bin\exam_server.exe --config config\server.conf --seed-demo --simulate 8
pause
