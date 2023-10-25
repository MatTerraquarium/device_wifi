https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/matter/getting_started/adding_clusters.html


https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/matter/getting_started/tools.html#ug-matter-tools-installing-zap

# START ZAP TOOL
zap ~/Projects/MATTER/template_exercise/src/template.zap --zcl ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/zcl/zcl.json --gen ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json

# GENERATE ZAP FILES
python3 ~/ncs/v2.4.2/modules/lib/matter/scripts/tools/zap/generate.py ~/Projects/MATTER/template_exercise/src/template.zap -t ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json -o ~/Projects/MATTER/template_exercise/src/zap-generated/

# CLEAR CHIP-TOOL CACHE
sudo rm -rf /tmp/chip_*

# COMMISION DEV IN MATTER WIFI
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug pairing ble-wifi 2 FibraFelice 0a1b2c3d4e 20202021 3840

# CHIP-TOOL COMMANDS
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff on 2 1
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug temperaturemeasurement read measured-value 2 1
~/Projects/MATTER_TOOLS/chip-tool-linux_2.4.1_x64/chip-tool-debug onoff off 2 1