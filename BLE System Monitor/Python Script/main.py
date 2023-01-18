# Python script to send hardware metrics to ESP32 device over BLE.
# Temperature sensors does not work on windows, so if you are on windows,
# comment line for temperature sensors data.
# Add your device MAC address to start the connection.
import sys
import asyncio
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError
import json
import time
import datetime
import psutil

# import struct
ADDRESS = "f4:12:fa:c0:8b:f1"

SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b'
infoCHARACTERISTIC_UUID = '67360b78-8c45-11ed-a1eb-0242ac120002'


def process_list(delay=1):
    proccesses = list(psutil.process_iter())
    procs = []
    try:
        for proc in proccesses:
            proc.cpu_percent(None)
        sys.stdout.flush()
        time.sleep(delay)

        for proc in proccesses:
            percent = proc.cpu_percent(None)
            if percent:
                procs.append((percent, proc.name()))
    except ProcessLookupError:
        pass
    except psutil.NoSuchProcess:
        pass

    res = sorted(procs, key=lambda x: x[0], reverse=True)[:5]
    return res


async def main(addr):
    device = await BleakScanner.find_device_by_address(addr, timeout=20.0)
    if not device:
        raise BleakError(f"A device with address {addr} could not be found.")
    async with BleakClient(device) as Client:
        print(f"Connected: {Client.is_connected}")
        while Client.is_connected():
            data = {}
            # Get System LocalTime
            ts = time.time()
            utc_offset = (datetime.datetime.fromtimestamp(ts) -
                          datetime.datetime.utcfromtimestamp(ts)).total_seconds()
            data['utc_offset'] = int(utc_offset)

            presentdate = datetime.datetime.now()
            unix_timestamp = datetime.datetime.timestamp(presentdate)
            data['local_time'] = int(unix_timestamp)

            # CPU Usage
            cpu_percent_cores = psutil.cpu_percent(interval=2, percpu=True)
            avg = sum(cpu_percent_cores) / len(cpu_percent_cores)
            data['cpu_percent_total'] = round(avg, 2)
            # MEM Usage
            RAM = psutil.virtual_memory().percent
            data['mem_usage'] = round(RAM, 2)
            # Battery Info
            battery = psutil.sensors_battery()
            battery_percentage = battery.percent if battery.percent is not None else 100
            data['battery_info'] = round(battery_percentage, 2)
            data['power_plugged'] = battery.power_plugged
            # temperature info
            temperatures = psutil.sensors_temperatures()
            core1_temp = temperatures['coretemp'][1].current
            data['core0_temp'] = core1_temp
            # add proc list to data
            data['cpu_top5_process'] = ['%.1f%% %s' % x for x in process_list()]

            await Client.write_gatt_char(infoCHARACTERISTIC_UUID, json.dumps(data).encode(), False)
            await asyncio.sleep(1)


if __name__ == "__main__":
    asyncio.run(main(ADDRESS))
