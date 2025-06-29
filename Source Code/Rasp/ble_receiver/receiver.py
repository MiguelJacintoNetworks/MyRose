import asyncio
import logging
import json
from pathlib import Path
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
from bleak import BleakScanner, BleakClient

from .parser import parse_packet, compute_checksum
from .writer import write_to_csv
from . import (
    HANDSHAKE_CHAR_UUID,
    DATA_CHAR_UUID,
    CMD_CHAR_UUID,
    DFR0022_ID,
    SERVO_ID,
    PIR_ID,
    SPEAKER_ID,
    TEMP_ID,
    SOIL_ID,
    PUMP_ID,
)

from data_publisher.config import CSV_DIR

CONFIRM_CSV = Path(CSV_DIR) / 'telemetry_confirmations.csv'

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

def build_upstream_packets(payload_json: str) -> bytes:
    data = json.loads(payload_json)
    mapping = {
        'DFR0022_DATA':      DFR0022_ID,
        'SERVO_DATA':        SERVO_ID,
        'PIR_DATA':          PIR_ID,
        'SPEAKER_DATA':      SPEAKER_ID,
        'TEMPERATURE_DATA':  TEMP_ID,
        'SOILMOISTURE_DATA': SOIL_ID,
        'PUMP_DATA':         PUMP_ID,
    }
    msg = bytearray()
    for field, sid in mapping.items():
        raw = data.get(field)
        if not raw:
            continue
        try:
            arr = json.loads(raw)
            value = int(arr[0].get('value', 0))
            lsb = value & 0xFF
            msb = (value >> 8) & 0xFF
            chk = compute_checksum(bytes([sid, lsb, msb]))
            msg += bytes([sid, lsb, msb, chk])
        except Exception:
            continue
    return bytes(msg)

class BLEDataReceiver:
    def __init__(self):
        self.loop = asyncio.get_running_loop()
        self.data_buffer = bytearray()
        self.handshake_complete = False
        self.ble_client = None

        class CSVHandler(FileSystemEventHandler):
            def __init__(self, parent):
                super().__init__()
                self.parent = parent
                self._last_size = CONFIRM_CSV.stat().st_size if CONFIRM_CSV.exists() else 0

            def on_modified(self, event):
                if event.src_path == str(CONFIRM_CSV):
                    new_size = CONFIRM_CSV.stat().st_size
                    if new_size > self._last_size:
                        lines = CONFIRM_CSV.read_text().splitlines()
                        raw_json = lines[-1].split(',', 2)[2]
                        packets = build_upstream_packets(raw_json)
                        if packets:
                            logging.info("[RECEIVER] BUILDING UPSTREAM PACKETS")
                            asyncio.run_coroutine_threadsafe(
                                self.parent._send_to_arduino(packets),
                                self.parent.loop
                            )
                        self._last_size = new_size

        self.csv_handler = CSVHandler(self)
        self.csv_observer = Observer()

    def process_buffer(self):
        while True:
            if len(self.data_buffer) < 2:
                break
            sid = self.data_buffer[0]
            if sid in (DFR0022_ID, TEMP_ID, SOIL_ID):
                expected_len = 4
            elif sid in (SERVO_ID, PIR_ID, SPEAKER_ID, PUMP_ID):
                expected_len = 3
            else:
                logging.error("[RECEIVER] UNKNOWN SENSOR ID %d", sid)
                del self.data_buffer[0]
                continue
            if len(self.data_buffer) < expected_len:
                break
            pkt = bytes(self.data_buffer[:expected_len])
            res = parse_packet(pkt)
            if res:
                sensor, payload = res
                write_to_csv(sensor, payload)
                logging.info("[RECEIVER] WRITTEN SENSOR %s PAYLOAD %s TO CSV", sensor, payload)
            else:
                logging.error("[RECEIVER] FAILED TO PARSE PACKET %s", pkt.hex().upper())
            del self.data_buffer[:expected_len]

    def notification_handler(self, sender, data):
        logging.info("[RECEIVER] RECEIVED DATA %s", data.hex().upper())
        self.data_buffer.extend(data)
        self.process_buffer()

    async def perform_handshake(self, client: BleakClient) -> bool:
        try:
            logging.info("[RECEIVER] SENDING HANDSHAKE 'READY'")
            await client.write_gatt_char(HANDSHAKE_CHAR_UUID, b"READY", response=True)
            await asyncio.sleep(1)
            resp = await client.read_gatt_char(HANDSHAKE_CHAR_UUID)
            if resp.decode().strip() == "READY":
                logging.info("[RECEIVER] HANDSHAKE SUCCESSFUL")
                self.handshake_complete = True
                return True
            logging.error("[RECEIVER] HANDSHAKE FAILED '%s'", resp.decode().strip())
            return False
        except Exception as e:
            logging.error("[RECEIVER] HANDSHAKE ERROR %s", str(e).upper())
            return False

    async def _send_to_arduino(self, data):
        if not self.ble_client or not self.ble_client.is_connected:
            logging.error("[RECEIVER] BLE CLIENT NOT CONNECTED - PAYLOAD DISCARDED")
            return
        max_chunk = 50
        total_chunks = (len(data) + max_chunk - 1)
        logging.info("[RECEIVER] SENDING UPSTREAM IN %d CHUNKS", total_chunks)
        for idx in range(0, len(data), max_chunk):
            chunk = data[idx:idx+max_chunk]
            try:
                await self.ble_client.write_gatt_char(CMD_CHAR_UUID, chunk, response=True)
                logging.info("[RECEIVER] SENT CHUNK %d/%d SIZE %d", idx//max_chunk+1, total_chunks, len(chunk))
                await asyncio.sleep(0.05)
            except Exception as e:
                logging.error("[RECEIVER] BLE SEND CHUNK FAILED %s", str(e).upper())
                return
        logging.info("[RECEIVER] UPSTREAM PACKETS SENT")

    async def connect_and_receive(self):
        logging.info("[RECEIVER] SEARCHING FOR DEVICE 'ARDUINOBLE'")
        device = None
        while device is None:
            try:
                devices = await BleakScanner.discover(timeout=5.0)
                for d in devices:
                    if d.name and d.name.upper() in ("ARDUINOBLE", "ARDUINO BLE"):
                        device = d
                        break
                if device is None:
                    logging.info("[RECEIVER] DEVICE NOT FOUND, RETRYING")
                    await asyncio.sleep(2)
            except Exception as e:
                logging.error("[RECEIVER] SCAN ERROR %s", str(e).upper())
                await asyncio.sleep(2)
        async with BleakClient(device.address, timeout=20.0) as client:
            if not client.is_connected:
                logging.error("[RECEIVER] FAILED TO CONNECT")
                return
            self.ble_client = client
            if not await self.perform_handshake(client):
                return
            if self.handshake_complete:
                self.csv_observer.schedule(self.csv_handler, path=str(CONFIRM_CSV.parent), recursive=False)
                self.csv_observer.start()
                await client.start_notify(DATA_CHAR_UUID, self.notification_handler)
                logging.info("[RECEIVER] NOTIFICATIONS ENABLED, WAITING FOR DATA")
                while client.is_connected:
                    await asyncio.sleep(1)
                await client.stop_notify(DATA_CHAR_UUID)
                self.csv_observer.stop()
                self.csv_observer.join()
                logging.info("[RECEIVER] NOTIFICATIONS STOPPED")

async def ble_receiver_loop():
    receiver = BLEDataReceiver()
    while True:
        try:
            await receiver.connect_and_receive()
        except Exception as e:
            logging.error("[RECEIVER] RECEIVER LOOP ERROR %s", str(e).upper())
        logging.info("[RECEIVER] RESTARTING SCAN IN 5 S")
        await asyncio.sleep(5)

if __name__ == "__main__":
    asyncio.run(ble_receiver_loop())