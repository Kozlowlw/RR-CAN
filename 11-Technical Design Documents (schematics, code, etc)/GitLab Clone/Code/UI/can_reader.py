#! /usr/bin/python3
import serial

def setup_serial(addr, baud):
    while True:
        try:
            s = serial.Serial(addr, baud)
            break

        except (serial.serialutil.SerialException, FileNotFoundError):
            _addr = input('Address not found... Enter to try again ({}):'.format(addr))
            if len(_addr) > 0:
                addr = _addr

    ready = False
    while not(ready):
        s.readline()
        ready = True;

    return s

def process_msg(msg):
    id_table = {'0x100': 'Temp Sensor', '0x101': 'Speed Sensor'}
    remote_request = False
    data = ''

    _msg = msg.split(' ')
    ID = _msg[2]
    if ID in id_table.values():
        ID = id_table[ID]

    print(_msg[-3:])

    if _msg[-3:] == ['REMOTE', 'REQUEST', 'FRAME']: # I know this is dumb.
        return {'ID': ID, 'Data': '', 'RR': True}
    
    return {'ID': ID, 'Data':  _msg[6], 'RR': False}

        

def main():
    arduino = setup_serial('/dev/tty.AMC0', 115200)

    while True:
        print(process_msg(arduino.read()))


if __name__ == '__main__':
    main()
    
