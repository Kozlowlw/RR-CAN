# SAE J1979 PIDs (Parameter IDs)
# Used to encode and decode data.


import json

PID_Data = json.load(open('data/PID.json'))
DTC_Data = json.load(open('data/DTC.json'))


def hexToDec(data):
    return int(data, 16)


def decToHex(data):
    return hex(round(data))[2:].zfill(2)


def truncate(hexString):
    return hexString.replace('/\s/g', '')


def getPID(hexString):
    response = hexString[0:1]
    hexString = truncate(hexString)
    mode = hexString[1:2]
    pid = hexToDec(hexString[3:4])
    if (mode == "1" or mode == "9") and response == "4":
        return (True,
                PID_Data[mode][pid]["pid"],
                PID_Data[mode][pid]["bytes"],
                PID_Data[mode][pid]["description"])
    else:
        return (False,
                0,
                0,
                "Non-Valid packet")


def toTwoDecimalPlaces(data):
    return round(data, 2)


def getLastDataByte(data):
    return data[-2:]


def getSecondLastDataBye(data):
    return data[-4:-2]


def getIndexedDataByte(index, data):
    return data[(index * 2):(index * 2) + 2]


def decodeCalculatedEngineLoad(data):
    return toTwoDecimalPlaces((100 / 255) * hexToDec(data))


def encodeCalculatedEngineLoad(data):
    return decToHex((255 / 100) * data)


def decodeTemperature(data):
    return toTwoDecimalPlaces(toTwoDecimalPlaces(hexToDec(data) - 40))


def encodeTemperature(data):
    return decToHex(data + 40)


def decodeShortTermFuelTrim(data):
    return toTwoDecimalPlaces((100 / 128) * hexToDec(data) - 100)


def encodeShortTermFuelTrim(data):
    return decToHex((128 / 100) * (data + 100))


def decodeFuelPressure(data):
    return toTwoDecimalPlaces(3 * hexToDec(data))


def encodeFuelPressure(data):
    return decToHex(data / 3)


def decodeIntakeManifoldAbsolutePressure(data):
    return toTwoDecimalPlaces(hexToDec(data))


def encodeIntakeManifoldAbsolutePressure(data):
    return decToHex(data)


def decodeEngineRPM(data):
    return toTwoDecimalPlaces(((256 * hexToDec(getSecondLastDataBye(data))) + hexToDec(getLastDataByte(data))) / 4)


def encodeEngineRPM(data):
    b = (data * 4) % 256
    a = (data * 4 - b) / 256
    return decToHex(a), decToHex(b)


def decodeVehicleSpeed(data):
    return toTwoDecimalPlaces(hexToDec(data))


def encodeVehicleSpeed(data):
    return decToHex(round(data))


def decodeTimingAdvance(data):
    return toTwoDecimalPlaces((hexToDec(data) / 2) - 64)


def encodeTimingAdvance(data):
    return decToHex((data + 64) * 2)


def decodeMafAirFlowRate(data):
    return toTwoDecimalPlaces(((256 * hexToDec(getSecondLastDataBye(data))) + hexToDec(getLastDataByte(data))) / 100)


def encodeMafAirFlowRate(data):
    b = (data * 100) % 256
    a = (data * 100 - b) / 256
    return decToHex(a), decToHex(b)


def decodeThrottlePosition(data):
    return toTwoDecimalPlaces((100 / 255) * hexToDec(data))


def encodeThrottlePosition(data):
    return decToHex(data * (255 / 100))


def decodeOxygenSensorVoltage(data):
    return toTwoDecimalPlaces(hexToDec(getSecondLastDataBye(data)))


def encodeOxygenSensorVoltage(data):
    return decToHex(data) + "00"


def decodeOxygenSensorShortTermFuelTrim(data):
    return toTwoDecimalPlaces(((100 / 128) * hexToDec(getLastDataByte(data))) - 100)


def encodeOxygenSensorShortTermFuelTrim(data):
    return decToHex((data + 100) * (128 / 100))


def decodeRunTimeSinceEngineStart(data):
    return toTwoDecimalPlaces((256 * hexToDec(getSecondLastDataBye(data))) + hexToDec(getLastDataByte(data)))


def encodeRunTimeSinceEngineStart(data):
    b = data % 256
    a = (data - b) / 256
    return decToHex(a), decToHex(b)


def decodeDistanceTravelledMilOn(data):
    return toTwoDecimalPlaces((256 * hexToDec(getSecondLastDataBye(data)) + hexToDec(getLastDataByte(data))))


def encodeDistanceTravelledMilOn(data):
    b = data % 256
    a = (data - b) / 256
    return decToHex(a), decToHex(b)


def decodeFuelRailPressure(data):
    return toTwoDecimalPlaces(0.079 * (256 * hexToDec(getSecondLastDataBye(data)) + hexToDec(getLastDataByte(data))))


def encodeFuelRailPressure(data):
    b = (data / 0.079) % 256
    a = (data / 0.079 - b) / 256
    return decToHex(a), decToHex(b)


def decodeFuelRailGaugePressure(data):
    return toTwoDecimalPlaces(10 * (256 * hexToDec(getSecondLastDataBye(data)) + hexToDec(getLastDataByte(data))))


def encodeFuelRailGaugePressure(data):
    b = (data / 10) % 256
    a = (data / 10 - b) / 256
    return decToHex(a), decToHex(b)


def decodeWarmupsSinceCodesCleared(data):
    return hexToDec(data)


def encodeWarmupsSinceCodesCleared(data):
    return decToHex(data)


def decodeDistanceTraveledSinceCodesCleared(data):
    return (256 * getSecondLastDataBye(hexToDec(data))) + getLastDataByte(hexToDec(data))


def encodeDistanceTraveledSinceCodesCleared(data):
    b = data % 256
    a = (data - b) / 256
    return decToHex(a), decToHex(b)


def decodeDataFromPID(PID, data):
    return {0x04: decodeCalculatedEngineLoad(data),
            0x05: decodeTemperature(data),
            0x06: decodeShortTermFuelTrim(data),
            0x0A: decodeFuelPressure(data),
            0x0B: decodeIntakeManifoldAbsolutePressure(data),
            0x0C: decodeEngineRPM(data),
            0x0D: decodeVehicleSpeed(data),
            0x0E: decodeTimingAdvance(data),
            0x10: decodeMafAirFlowRate(data),
            0x11: decodeThrottlePosition(data),
            0x14: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x15: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x16: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x17: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x18: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x19: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x1A: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x1B: (decodeOxygenSensorVoltage(data), decodeOxygenSensorShortTermFuelTrim(data)),
            0x1F: decodeRunTimeSinceEngineStart(data),
            0x21: decodeDistanceTravelledMilOn(data),
            0x22: decodeFuelRailPressure(data),
            0x23: decodeFuelRailGaugePressure(data),
            0x30: decodeWarmupsSinceCodesCleared(data),
            0x31: decodeDistanceTraveledSinceCodesCleared(data)
            }.get(PID, hexToDec(data))


def encodeDataToPID(PID, data):
    return {0x04: encodeCalculatedEngineLoad(data),
            0x05: encodeTemperature(data),
            0x06: encodeShortTermFuelTrim(data),
            0x0A: encodeFuelPressure(data),
            0x0B: encodeIntakeManifoldAbsolutePressure(data),
            0x0C: encodeEngineRPM(data),
            0x0D: encodeVehicleSpeed(data),
            0x0E: encodeTimingAdvance(data),
            0x10: encodeMafAirFlowRate(data),
            0x11: encodeThrottlePosition(data),
            0x14: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x15: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x16: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x17: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x18: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x19: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x1A: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x1B: (encodeOxygenSensorVoltage(data), encodeOxygenSensorShortTermFuelTrim(data)),
            0x1F: encodeRunTimeSinceEngineStart(data),
            0x21: encodeDistanceTravelledMilOn(data),
            0x22: encodeFuelRailPressure(data),
            0x23: encodeFuelRailGaugePressure(data),
            0x30: encodeWarmupsSinceCodesCleared(data),
            0x31: encodeDistanceTraveledSinceCodesCleared(data)
            }.get(PID, decToHex(data))


# Print statement for debugging
def testString(value, a, b, c):
    print(value + " : a=" + str(a) + ", b=" + str(b) + ", c=" + str(c))


def test():
    print("Testing decode and encode")
    a = "F0AB"
    b = decodeCalculatedEngineLoad(a)
    c = encodeCalculatedEngineLoad(b)
    testString("CalculatedEngineLoad", a, b, c)
    b = decodeTemperature(a)
    c = encodeTemperature(b)
    testString("Temperature", a, b, c)
    b = decodeShortTermFuelTrim(a)
    c = encodeShortTermFuelTrim(b)
    testString("ShortTermFuelTrim", a, b, c)
    b = decodeFuelPressure(a)
    c = encodeFuelPressure(b)
    testString("FuelPressure", a, b, c)
    b = decodeIntakeManifoldAbsolutePressure(a)
    c = encodeIntakeManifoldAbsolutePressure(b)
    testString("IntakeManifoldAbsolutePressure", a, b, c)
    b = decodeEngineRPM(a)
    c = encodeEngineRPM(b)
    testString("EngineRPM", a, b, c[0] + c[1])
    b = decodeVehicleSpeed(a)
    c = encodeVehicleSpeed(b)
    testString("VehicleSpeed", a, b, c)
    b = decodeTimingAdvance(a)
    c = encodeTimingAdvance(b)
    testString("TimingAdvance", a, b, c)
    b = decodeMafAirFlowRate(a)
    c = encodeMafAirFlowRate(b)
    testString("MafAirFlowRate", a, b, c[0] + c[1])
    b = decodeThrottlePosition(a)
    c = encodeThrottlePosition(b)
    testString("ThrottlePosition", a, b, c)
    b = decodeOxygenSensorVoltage(a)
    c = encodeOxygenSensorVoltage(b)
    testString("OxygenSensorVoltage", a, b, c)
    b = decodeOxygenSensorShortTermFuelTrim(a)
    c = encodeOxygenSensorShortTermFuelTrim(b)
    testString("OxygenSensorShortTermFuelTrim", a, b, c)
    b = decodeRunTimeSinceEngineStart(a)
    c = encodeRunTimeSinceEngineStart(b)
    testString("DistanceTravelledMilOn", a, b, c[0] + c[1])
    b = decodeFuelRailPressure(a)
    c = encodeFuelRailPressure(b)
    testString("FuelRailPressure", a, b, c[0] + c[1])
    b = decodeFuelRailGaugePressure(a)
    c = encodeFuelRailGaugePressure(b)
    testString("FuelRailGaugePressure", a, b, c[0] + c[1])
    x = "41043C"  # Should be valid/True
    print(getPID(x)[0], getPID(x)[1], getPID(x)[2], getPID(x)[3])
    x = "44043C"  # Should be invalid/False
    print(getPID(x)[0], getPID(x)[1], getPID(x)[2], getPID(x)[3])


if __name__ == "__main__":
    test()
