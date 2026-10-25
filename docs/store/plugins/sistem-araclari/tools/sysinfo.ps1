[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$os = Get-CimInstance Win32_OperatingSystem
$cpu = Get-CimInstance Win32_Processor
Write-Output "OS: $($os.Caption) ($($os.OSArchitecture))"
Write-Output "Kernel: $($os.Version)"
Write-Output "CPU: $($cpu.Name) ($($cpu.NumberOfCores) çekirdek)"
Write-Output ""
$mem = [math]::Round($os.TotalVisibleMemorySize / 1MB, 2)
$free = [math]::Round($os.FreePhysicalMemory / 1MB, 2)
Write-Output "Bellek: Toplam ${mem} GB, Boş ${free} GB"
Write-Output ""
Get-PSDrive -PSProvider FileSystem | ForEach-Object {
    $total = [math]::Round($_.Used / 1GB + $_.Free / 1GB, 1)
    $freeGB = [math]::Round($_.Free / 1GB, 1)
    Write-Output "$($_.Root) Toplam: ${total}GB, Boş: ${freeGB}GB"
}
