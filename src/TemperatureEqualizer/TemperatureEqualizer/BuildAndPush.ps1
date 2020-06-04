param( $target = "64.227.77.146")
$name = (Get-ItemProperty .).Name
pushd
dotnet publish -r linux-x64
cd bin\Debug\netcoreapp3.1\linux-x64\publish

rsync -avhr --chmod=a=rw,Da+x -e ssh * root@${target}:/bin/alcomij/TemperatureEqualizer
ssh root@${target} bash /bin/alcomij/TemperatureEqualizer/Scripts/Install.sh
popd
