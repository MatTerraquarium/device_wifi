https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/matter/getting_started/adding_clusters.html


https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/matter/getting_started/tools.html#ug-matter-tools-installing-zap

# START ZAP TOOL
zap ~/Projects/MATTER/template_exercise/src/template.zap --zcl ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/zcl/zcl.json --gen ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json

# GENERATE ZAP FILES
python3 ~/ncs/v2.4.2/modules/lib/matter/scripts/tools/zap/generate.py ~/Projects/MATTER/template_exercise/src/template.zap -t ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json -o ~/Projects/MATTER/template_exercise/src/zap-generated/

# CLEAR CHIP-TOOL CACHE
sudo rm -rf /tmp/chip_*

# COMMISION DEV IN MATTER WIFI
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug pairing ble-wifi 1 WiFi-SSID WiFi-PASS 20202021 3840

# CHIP-TOOL COMMANDS
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 1 2
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 1 2

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 1 3
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 1 3

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 1 4
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 1 4

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 1 5
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 1 5

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 1 6
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 1 6

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug temperaturemeasurement read measured-value 1 7
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug relativehumiditymeasurement read measured-value 1 8

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug temperaturemeasurement read measured-value 1 9
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug relativehumiditymeasurement read measured-value 1 10

~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug temperaturemeasurement read measured-value 1 11
