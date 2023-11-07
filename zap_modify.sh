#!/bin/bash
echo "Start ZAP Toooland edit ZAP file"
zap ~/Projects/MATTER/device_wifi/src/template.zap --zcl ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/zcl/zcl.json --gen ~/ncs/v2.4.2/modules/lib/matter/src/app/zap-templates/app-templates.json
