#!/usr/bin/python3
"""
Graph functions for plotting CAN bus data
"""

from matplotlib import pyplot as plt
from matplotlib import style
import csv
import copy

'''
Graph mode
'''
graph_mode = 2

arbid = 0x009
arbid_first = 0x000
arbid_last = 0x800
arbid_to_check = [i for i in range(arbid_last)]
#arbid_to_check = [0x007]
filename = 'test.txt'


def get_data_by_pid(pid, file_name):
    x = []
    y = []
    with open(file_name, 'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=' ')
        for row in plots:
            if int(row[2][:3]) == pid:
                x.append(float(row[0][1:-1]))
                y.append(int(row[2][4:], 16))
    return pid, x, y


def get_data_by_pid_and_byte(pid, byte, file_name):
    x = []
    y = []
    print("Checking " + file_name + " for ID#%X" % arbid)
    with open(file_name, 'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=' ')
        for row in plots:
            if int(row[2][:3]) == pid:
                x.append(float(row[0][1:-1]))
                y.append(int(row[2][4+byte*2:4+byte*2+2], 16))
    return pid, x, y


def get_multi_data_by_pid(pids, file_name):
    tup = [(0, [], [])]*arbid_last
    x_t = []
    y_t = []

    for i in range(arbid_last):
        x_t.append([])
        y_t.append([])

    with open(file_name, 'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=' ')
        for row in plots:
            if int(row[2][:3]) in pids:
                x_t[int(row[2][:3])].append(float(row[0][1:-1]))
                y_t[int(row[2][:3])].append(int(row[2][4:], 16))

    for i in range(arbid_last):
        if len(x_t[i]) != 0:
            tup[i] = (i, x_t[i], y_t[i])
    tup = [x for x in tup if x != (0, [], [])]
    return tup


def get_multi_data_by_pid_and_byte(pids, byte, file_name):
    tup = [(0, [], [])]*arbid_last
    x_t = []
    y_t = []

    for i in range(arbid_last):
        x_t.append([])
        y_t.append([])

    with open(file_name, 'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=' ')
        for row in plots:
            if int(row[2][:3]) in pids:
                x_t[int(row[2][:3])].append(float(row[0][1:-1]))
                y_t[int(row[2][:3])].append(int(row[2][4+byte*2:4+byte*2+2], 16))

    for i in range(arbid_last):
        if len(x_t[i]) != 0:
            tup[i] = (i, x_t[i], y_t[i])
    tup = [x for x in tup if x != (0, [], [])]
    return tup


def format_tuple(tup, getkey, rev=False):
    return sorted(tup, key=getkey, reverse=rev)


def get_multi_data_by_pid_and_split(pids, file_name):
    tup = get_multi_data_by_pid(pids, file_name)
    tup = sorted(tup, key=get_size)
    tup2 = copy.deepcopy(tup)
    tup2 = sorted(tup2, key=get_size, reverse=True)
    fig = plt.figure()
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)
    for i in range(min(len(tup), 15)):
        ax1.plot(tup2[i][1], tup2[i][2], label='ID#%.3X' % int(tup2[i][0]))
    for i in range(min(len(tup2), 15)):
        ax2.plot(tup[i][1], tup[i][2], label='ID#%.3X' % int(tup[i][0]))
    ax1.legend()
    ax1.set_title('Most used CAN IDs')
    ax2.set_title('Least used CAN IDs')
    ax2.legend()
    plt.show()


def get_multi_data_by_pid_and_byte_and_split(pids, byte, file_name):
    tup = get_multi_data_by_pid_and_byte(pids, byte, file_name)
    tup = sorted(tup, key=get_size)
    tup2 = copy.deepcopy(tup)
    tup2 = sorted(tup2, key=get_size, reverse=True)
    fig = plt.figure()
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)
    for i in range(min(len(tup), 15)):
        ax1.plot(tup2[i][1], tup2[i][2], label='ID#%.3X' % int(tup2[i][0]))
    for i in range(min(len(tup2), 15)):
        ax2.plot(tup[i][1], tup[i][2], label='ID#%.3X' % int(tup[i][0]))
    ax1.legend()
    ax1.set_title('Most used CAN IDs by byte #' + str(byte))
    ax2.set_title('Least used CAN IDs by byte #' + str(byte))
    ax2.legend()
    plt.show()


def get_size(item):
    return len(item[2])


def main():
    style.use('fivethirtyeight')
    print("Graphing your CAN logs, one line at a time")
    if graph_mode == 0:
        print(arbid_to_check)
        tup = get_multi_data_by_pid_and_byte(arbid_to_check, 0, filename)
        for i in range(len(tup)):
            if i in arbid_to_check:
                plt.plot(tup[i][1], tup[i][2], label='ID#%.3X' % int(tup[i][0]))
        print(sorted(tup, key=get_size, reverse=False))
        plt.xlabel('Time (s)')
        plt.ylabel('Data')
        plt.title('CAN data from ' + filename)
        plt.legend()
        plt.show()
    elif graph_mode == 1:
        get_multi_data_by_pid_and_split(arbid_to_check, filename)
    elif graph_mode == 2:
        get_multi_data_by_pid_and_byte_and_split(arbid_to_check, 1, filename)

if __name__ == "__main__":
    main()
