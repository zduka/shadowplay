#!/bin/bash
echo "Syncing source code..."
rsync -r -ssh src include platformio.ini peta@10.0.0.38:/home/peta/devel/shadowplay --delete
echo "Buiding avr-src"
ssh peta@10.0.0.38 "cd ~/devel/shadowplay && pio run --target upload"
