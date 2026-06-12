$webhookUrl = "[https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9](https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9)"
$telemetryOn = $true

$payload = @{
    content = "install started somewhere in the universe"
}

if ($telemetryOn) {
    $osPlatform = (Get-CimInstance Win32_OperatingSystem).Caption
    $psVersion = $PSVersionTable.PSVersion.ToString()

    # Generates the backticks safely without breaking the chat layout
    $3ticks = [string][char]96 * 3
    
    $markdown = "**Midnight Setup Initiated**`n" + 
                $3ticks + "yaml`n" + 
                "OS Platform: $osPlatform`n" + 
                "User Agent: PowerShell/$psVersion`n" + 
                "Status: Setup cache verified & runtime started.`n" + 
                $3ticks

    $payload = @{
        content = $markdown
    }
}

$jsonPayload = ConvertTo-Json $payload -Depth 4

try {
    Invoke-RestMethod -Uri $webhookUrl -Method Post -ContentType "application/json" -Body $jsonPayload
    Write-Host "Webhook sent successfully!" -ForegroundColor Green
}
catch {
    Write-Warning "Webhook silent fail: $_"
}
