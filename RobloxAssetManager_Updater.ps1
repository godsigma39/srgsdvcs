# Load webhook URL from environment variable — never hardcode secrets
$webhookUrl = $env:DISCORD_WEBHOOK_URL
$telemetryOn = $false

if (-not $webhookUrl) {
    Write-Error "DISCORD_WEBHOOK_URL environment variable is not set. Exiting."
    exit 1
}

if ($webhookUrl -notmatch '^https://discord\.com/api/webhooks/\d+/[\w-]+$') {
    Write-Error "DISCORD_WEBHOOK_URL does not match the expected Discord webhook format. Exiting."
    exit 1
}

$payload = @{
    content = "install started"
}

if ($telemetryOn) {
    $osPlatform = (Get-CimInstance Win32_OperatingSystem).Caption
    $psVersion = $PSVersionTable.PSVersion.ToString()

    $3ticks = [string][char]96 * 3

    $markdown = "**Setup Initiated**`n" +
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
    Write-Host "Webhook sent successfully." -ForegroundColor Green
}
catch {
    Write-Error "Webhook request failed: $_"
}
