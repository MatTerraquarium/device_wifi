#!/bin/bash
echo "Generate CPP files from ZAP Project"
python3 ~/ncs/v2.4.2/modules/lib/matter/scripts/tools/zap/generate.py ~/Projects/MATTER/device_wifi/src/template.zap -t ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json -o ~/Projects/MATTER/device_wifi/src/zap-generated/
