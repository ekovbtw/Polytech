#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wscapi.h>
#include <iostream>
#pragma comment(lib, "wscapi.lib")

const char* health_to_str(WSC_SECURITY_PROVIDER_HEALTH h)
{
    switch (h) {
    case WSC_SECURITY_PROVIDER_HEALTH_GOOD:         return "GOOD";
    case WSC_SECURITY_PROVIDER_HEALTH_NOTMONITORED: return "NOT MONITORED";
    case WSC_SECURITY_PROVIDER_HEALTH_POOR:         return "POOR";
    case WSC_SECURITY_PROVIDER_HEALTH_SNOOZE:       return "SNOOZE";
    default:                                        return "UNKNOWN";
    }
}

struct ProviderInfo {
    WSC_SECURITY_PROVIDER provider;
    const char* name;
};

ProviderInfo providers[] = {
    { WSC_SECURITY_PROVIDER_ANTIVIRUS,            "Antivirus"          },
    { WSC_SECURITY_PROVIDER_FIREWALL,             "Firewall"           },
    { WSC_SECURITY_PROVIDER_ANTISPYWARE,          "Antispyware"        },
    { WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS,  "Auto Update"        },
    { WSC_SECURITY_PROVIDER_INTERNET_SETTINGS,    "Internet Settings"  },
    { WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL, "User Account Control"},
    { WSC_SECURITY_PROVIDER_SERVICE,              "Security Center Service"}
};

int main()
{
    int count = sizeof(providers) / sizeof(providers[0]);

    WSC_SECURITY_PROVIDER_HEALTH prev[16];

   
    std::cout << "=== Start status ===" << std::endl;
    for (int i = 0; i < count; i++)
    {
        HRESULT hr = WscGetSecurityProviderHealth(providers[i].provider, &prev[i]);
        if (FAILED(hr)) prev[i] = WSC_SECURITY_PROVIDER_HEALTH_NOTMONITORED;
        std::cout << providers[i].name << ": " << health_to_str(prev[i]) << std::endl;
    }

    std::cout << "\n...\n" << std::endl;

   
    while (true)
    {
        Sleep(1000);  

        for (int i = 0; i < count; i++)
        {
            WSC_SECURITY_PROVIDER_HEALTH curr;
            HRESULT hr = WscGetSecurityProviderHealth(providers[i].provider, &curr);
            if (FAILED(hr)) prev[i] = WSC_SECURITY_PROVIDER_HEALTH_NOTMONITORED;

            // сравниваем с предыдущим
            if (curr != prev[i])
            {
                std::cout << "[CHANGE] " << providers[i].name << ": "
                    << health_to_str(prev[i]) << " -> " << health_to_str(curr)
                    << std::endl;
                prev[i] = curr;   
            }
        }
    }
    return 0;
}