@echo off
cd /d %~dp0..
if not exist config\server.conf copy config\server.example.conf config\server.conf
echo Starting Smart Exam Lab server (simulation of 8 PCs + demo data)...
echo For real lab mode without simulation, run: bin\exam_server.exe --config config\server.conf
bin\exam_server.exe --config config\server.conf --seed-demo --simulate 8
pause
