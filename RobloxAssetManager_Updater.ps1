# Webhook Setup
$webhookUrl = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9"

# Simulating the telemetry toggle (Set to $false to turn off)
$telemetryOn = $true 

# Default payload
$payload = @{
    content = "install started somewhere in the universe"
}

# If telemetry is enabled, gather system info
if ($telemetryOn) {
    # Grabbing OS details natively since navigator.platform doesn't exist here
    $osPlatform = (Get-CimInstance Win32_OperatingSystem).Caption
    $psVersion = $PSVersionTable.PSVersion.ToString()

    $payload = @{
        content = "**Midnight Setup Initiated**`n\`\`\`yaml`nOS Platform: $osPlatform`nUser Agent: PowerShell/$psVersion`nStatus: Setup cache verified & runtime started.`n\`\`\`"
    }
}

# Convert the payload to JSON data
$jsonPayload = ConvertTo-Json $payload -Depth 4

# Send it to Discord
try {
    Invoke-RestMethod -Uri $webhookUrl -Method Post -ContentType "application/json" -Body $jsonPayload
    Write-Host "Webhook sent successfully!" -ForegroundColor Green
}
catch {
    Write-Warning "Webhook silent fail: $_"
}
