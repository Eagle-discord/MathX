# run this once from the mathx-qt project root
# PowerShell: Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

$fontsDir = "$PSScriptRoot\resources\fonts"
New-Item -ItemType Directory -Force -Path $fontsDir | Out-Null

$base = "https://github.com/JetBrains/JetBrainsMono/raw/master/fonts/ttf"
$files = @(
    "JetBrainsMono-Regular.ttf",
    "JetBrainsMono-Medium.ttf",
    "JetBrainsMono-Bold.ttf",
    "JetBrainsMono-ExtraBold.ttf"
)

foreach ($f in $files) {
    $dest = Join-Path $fontsDir $f
    Write-Host "Downloading $f ..."
    Invoke-WebRequest -Uri "$base/$f" -OutFile $dest -UseBasicParsing
    Write-Host "  -> $($(Get-Item $dest).Length) bytes"
}

Write-Host ""
Write-Host "Done! Now rebuild in Visual Studio (Build -> Build All)."
