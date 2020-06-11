#!/bin/sh

workingDir="/bin/alcomij/TemperatureEqualizer/"
serviceName="TemperatureEqualizer"
fileExec="TemperatureEqualizer"
description="Equalizes greenhouse temperature by controlling fans individually"
fileFullExec=$workingDir$fileExec
chmod +x $fileFullExec 
sh $workingDir/Scripts/SetBootService.sh $serviceName $fileFullExec $workingDir $description
