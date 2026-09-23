param(
    [string]$JuceDir = $env:JUCE_DIR,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-vs'
if (-not $JuceDir) { $JuceDir = Join-Path $projectRoot 'external\JUCE' }
if (-not (Test-Path -LiteralPath (Join-Path $JuceDir 'CMakeLists.txt'))) {
    throw 'Supply a JUCE 8.0.12 checkout with -JuceDir or place it in external\JUCE.'
}
$configureArguments = @('-S', $projectRoot, '-B', $buildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DJUCE_DIR=$JuceDir")
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
Write-Output (Join-Path $buildDirectory "ButtmeisterTunerAnalyzer_artefacts\$Configuration")
