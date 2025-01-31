$ts = (Get-Date).ToString("yyyyMMdd-HHmmss")
$compress = @{
  Path = "Win32\*.exe", "Win32\*.dll", "x64\*.exe", "x64\*.dll", "ARM64\*.exe", "ARM64\*.dll"
  DestinationPath = "soordp-$ts.zip"
}
Compress-Archive @compress