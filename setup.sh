#! /bin/bash
serial_tool -s -P "/dev/ttyACM0" -l "$HOME/Documents/pt_log" --mysql-table "pt" & sudo python ./Thermocouple-Data-Node/max31856.py > /dev/null && fg
