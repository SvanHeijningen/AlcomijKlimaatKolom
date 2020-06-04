#!/bin/bash
serviceName=$1
fileExec=$2
workingDir=$3
description=$4
if [ -z "$serviceName" ]
then
  echo "Arg1: Service name empty"
  exit
fi

if [ -z "$fileExec" ]
then
  echo "Arg2: Executable empty"
  exit
fi
if [ -z "$workingDir" ]
then
  echo "Arg3: WorkingDir empty"
  exit
fi
if [ -z "$description" ]
then
  echo "Arg4 Description empty"
  exit
fi

fileService="/etc/systemd/system/$serviceName.service"
if [ -f $fileService ]
then
  echo "Remove $fileService"
  rm $fileService
fi

environmentConfig="/etc/alcomij/$serviceName.conf"

echo "[Unit]
Description=$description
Wants=network.target
After=syslog.target network-online.target
[Service]
Type=simple
WorkingDirectory=$workingDir
EnvironmentFile=$environmentConfig
ExecStart=$fileExec --username=\$THINGSBOARDUSER --password=\$THINGSBOARDPASS
Restart=on-failure
RestartSec=10
KillMode=process
[Install]
WantedBy=multi-user.target
" > $fileService

echo "File $fileExec executable"
chmod +x "$fileExec" || exit

if ! [ -f $environmentConfig ]
then
echo Creating sample $environmentConfig
echo "ASPNETCORE_ENVIRONMENT=Development
ALCOMIJDIR=/etc/alcomij
THINGSBOARDUSER=s.vanheijningen@alcomij.nl
THINGSBOARDPASS="thepassword!!&"
" > $environmentConfig
fi
echo "Start service"
systemctl enable $serviceName.service || exit
systemctl start $serviceName.service || exit
echo "Service Started"
