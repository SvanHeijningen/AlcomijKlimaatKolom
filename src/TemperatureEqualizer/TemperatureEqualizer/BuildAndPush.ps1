param( $target = "64.227.77.146")
$name = (Get-ItemProperty .).Name
pushd
dotnet publish -r linux-x64
cd bin\Debug\netcoreapp3.1\linux-x64\publish
scp * root@${target}:test-equalize
ssh root@${target} chmod +x test-equalize/TemperatureEqualizer
popd
