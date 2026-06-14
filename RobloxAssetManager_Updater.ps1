$ErrorActionPreference = 'Stop'

$webhookUrl = "[https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9](https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9)"
$telemetryOn = $true

$payload = @{
    content = "install started somewhere in the universe"
}

if ($telemetryOn) {
    try {
        $osPlatform = (Get-CimInstance Win32_OperatingSystem).Caption
    }
    catch {
        Write-Warning "Failed to retrieve OS platform: $($_.Exception.Message)"
        $osPlatform = "Unknown"
    }

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

try {
    $jsonPayload = ConvertTo-Json $payload -Depth 4
}
catch {
    Write-Error "Failed to serialize payload to JSON: $($_.Exception.Message)"
    exit 1
}

try {
    Invoke-RestMethod -Uri $webhookUrl -Method Post -ContentType "application/json" -Body $jsonPayload
    Write-Host "Webhook sent successfully!" -ForegroundColor Green
}
catch {
    $statusCode = $null
    if ($_.Exception.Response) {
        $statusCode = [int]$_.Exception.Response.StatusCode
    }
    $errorDetail = "Webhook request failed: $($_.Exception.Message)"
    if ($statusCode) {
        $errorDetail += " (HTTP $statusCode)"
    }
    Write-Error $errorDetail
    exit 1
}
