#!/usr/bin/env python3

import asyncio
import logging
import signal
import sys

from ble_receiver.receiver import ble_receiver_loop
from data_publisher.publisher import publisher_loop

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

async def main():
    logging.info("[DEPLOY] STARTING BLE RECEIVER LOOP AND MQTT PUBLISHER LOOP")
    tasks = [
        asyncio.create_task(ble_receiver_loop(), name="BLE_RECEIVER"),
        asyncio.create_task(publisher_loop(),  name="MQTT_PUBLISHER")
    ]
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.create_task(shutdown(tasks)))
    await asyncio.gather(*tasks)

async def shutdown(tasks):
    logging.info("[DEPLOY] SHUTDOWN SIGNAL RECEIVED - CANCELLING TASKS")
    for t in tasks:
        t.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)
    logging.info("[DEPLOY] ALL TASKS CANCELLED - EXITING")
    sys.exit(0)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        logging.error("[DEPLOY] UNEXPECTED ERROR: %s", str(e).upper())
        sys.exit(1)