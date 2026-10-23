@echo off
cd /d %~dp0..
if not exist config\client.conf copy config\client.example.conf config\client.conf
echo Starting client agent. Edit config\client.conf first: unique pc_id + server IP.
bin\exam_client.exe --config config\client.conf
pause
