$base = "C:\Users\humna\Desktop\Parallel and Distributed Computing\PROJECT\gossip-can-hybrid"
$src  = "$base\src"
$out  = "$base\build\gossip_can_hybrid.exe"

if (!(Test-Path "$base\build")) { New-Item -ItemType Directory "$base\build" | Out-Null }

$files = @(
    "$src\main.cpp",
    "$src\simulator\EventQueue.cpp",
    "$src\simulator\Simulator.cpp",
    "$src\network\Node.cpp",
    "$src\metrics\MetricsCollector.cpp",
    "$src\metrics\CSVExporter.cpp",
    "$src\can\Zone.cpp",
    "$src\can\CANOverlay.cpp",
    "$src\can\TakeoverManager.cpp",
    "$src\gossip\MembershipList.cpp",
    "$src\gossip\GossipLayer.cpp"
)

$args2 = @("-std=c++17", "-Wall", "-Wextra", "-I$src") + $files + @("-o", $out)
& "C:\msys64\ucrt64\bin\g++.exe" @args2

if ($LASTEXITCODE -eq 0) {
    Write-Host "BUILD OK"
    Write-Host "Running..."
    & $out
} else {
    Write-Host "BUILD FAILED (exit $LASTEXITCODE)"
}
