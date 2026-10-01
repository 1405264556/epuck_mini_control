param(
    [string]$QtRoot = 'F:\Qt\6.10.3\mingw_64',
    [string]$CompilerBin = 'F:\Tools\mingw64\mingw64\bin'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$versionLine = Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(epuck_mini_control VERSION ([0-9.]+)'
if (-not $versionLine) { throw 'Project version not found.' }
$version = $versionLine.Matches[0].Groups[1].Value
$source = Join-Path $root 'build\mingw-release\bin\epuck_mini_control.exe'
if (-not (Test-Path -LiteralPath $source)) { throw 'Build mingw-release before packaging.' }
$output = Join-Path $root 'release\epuck_mini_control'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $output -Force
$env:PATH = "$QtRoot\bin;$CompilerBin;" + $env:PATH
& (Join-Path $QtRoot 'bin\windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $output 'epuck_mini_control.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path $CompilerBin $dll) -Destination $output -Force
}
foreach ($name in @('README.md', 'CHANGELOG.md', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $output -Force
}
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $output -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'examples') -Destination $output -Recurse -Force
$launcher = Get-ChildItem -LiteralPath $root -Filter '*.cmd' | Where-Object { $_.Name -like '*e-puck Mini*' } | Select-Object -First 1
Copy-Item -LiteralPath $launcher.FullName -Destination $output -Force
Copy-Item -Path (Join-Path $root '*.url') -Destination $output -Force
New-Item -ItemType Directory -Path (Join-Path $output 'algorithms') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'docs\algorithm_plugins.txt') -Destination (Join-Path $output 'algorithms\README.txt') -Force
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut([IO.Path]::ChangeExtension($launcher.FullName, 'lnk'))
$shortcut.TargetPath = Join-Path $output 'epuck_mini_control.exe'
$shortcut.WorkingDirectory = $output
$shortcut.IconLocation = "$output\epuck_mini_control.exe,0"
$shortcut.Description = "e-puck Mini Control $version"
$shortcut.Save()
$archive = Join-Path $root "release\epuck_mini_control-v$version-win64.zip"
Compress-Archive -LiteralPath $output -DestinationPath $archive -Force
Get-FileHash -LiteralPath $archive -Algorithm SHA256 | Format-List
Write-Output "Package: $archive"
