$ts = (Get-Date).ToString("yyyyMMdd-HHmmss")
$compress = @{
  Path = "Win32\*.pdb", "x64\*.pdb"
  DestinationPath = "soordp-pdb-$ts.zip"
}
Compress-Archive @compress