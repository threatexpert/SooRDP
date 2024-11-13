$ts = (Get-Date).ToString("yyyyMMdd-HHmmss")
$compress = @{
  Path = "Win32\*.exe", "Win32\*.dll", "x64\*.exe", "x64\*.dll"
  DestinationPath = "soordp-$ts.zip"
}
Compress-Archive @compress